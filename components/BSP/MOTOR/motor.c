/**
 * @file    motor.c
 * @brief   和面机全电机控制实现
 *
 * 电机控制架构:
 *   - 步进电机:  GPIO bit-bang + GPTimer 硬件定时器产生精确脉冲
 *   - ESC 电机:   LEDC 50Hz PWM, 1~2ms 脉宽控制转速
 *   - 继电器:     GPIO 高低电平直接控制
 *   - 舵机:       LEDC 50Hz PWM, 0.5~2.5ms 脉宽控制角度
 *
 * 紧急停止机制:
 *   1. g_emergency_stop 全局标记 — 电机协同函数每步前检查
 *   2. vTaskDelete 销毁电机任务 — 从外部强制终止
 */

#include "motor.h"
#include "gptim.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"

static const char *TAG = "MOTOR";

/** 步进电机自动往返模式的 FreeRTOS 任务句柄 */
static TaskHandle_t step_auto_task_handle = NULL;

/** 紧急停止标记: 置位后电机协同函数在各步骤间检测并提前退出 */
static volatile bool g_emergency_stop = false;

/**
 * @brief 步进电机往返运行任务
 */
static void step_auto_task(void *pvParameters)
{
    while (1) {
        // 正转 180 度
        step_rotate_angle(180, DIR_CW, 1000);
        vTaskDelay(pdMS_TO_TICKS(500));
        // 反转 180 度
        step_rotate_angle(180, DIR_CCW, 1000);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void step_start_auto_mode(void)
{
    if (step_auto_task_handle == NULL) {
        xTaskCreate(step_auto_task, "step_auto", 4096, NULL, 5, &step_auto_task_handle);
        ESP_LOGI(TAG, "Stepper auto mode started");
    }
}

void step_stop_auto_mode(void)
{
    if (step_auto_task_handle != NULL) {
        vTaskDelete(step_auto_task_handle);
        step_auto_task_handle = NULL;
        step_stop();
        ESP_LOGI(TAG, "Stepper auto mode stopped");
    }
}

/**
 * @brief 紧急停止 — 立即停止所有电机并销毁自动任务
 *
 * 调用时机: EEZ UI 紧急按钮 / HTTP POST /api/stop / 串口/TCP STOP 指令
 * 行为:
 *   1. 置 g_emergency_stop = true → 电机协同函数检测后提前返回
 *   2. 销毁步进自动往返任务 (如果运行中)
 *   3. 停止步进脉冲输出
 *   4. 逐一停止所有电机硬件
 */
void motor_emergency_stop(void)
{
    g_emergency_stop = true;
    step_stop_auto_mode();
    step_stop();
    pump_stop();
    grinder_stop();
    mixer_esc_stop();
    dough_esc_stop();
    ESP_LOGI(TAG, "EMERGENCY STOP EXECUTED");
}

//步进电机函数实现
void step_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << step_stp) | (1ULL << step_dir) | (1ULL << step_en),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    
    gpio_set_level(step_en, 1);
    gpio_set_level(step_dir, 0);
    gpio_set_level(step_stp, 0);
    
    gptim_init(step_stp);
    motor_pwm_init();
    relay_init();
    
    ESP_LOGI(TAG, "Stepper motor and PWM motors and Relay motors initialized");
}

void step_set_dir(uint8_t dir)
{
    gpio_set_level(step_dir, dir);
}

void step_enable(void)
{
    gpio_set_level(step_en, 0);
}

void step_disable(void)
{
    gpio_set_level(step_en, 1);
}

/**
 * @brief 步进电机旋转指定角度 (阻塞式)
 * @param angle    角度 (度), 正值
 * @param dir      方向 DIR_CW / DIR_CCW
 * @param speed_us 脉冲间隔 (微秒), 值越小越快, 典型 1000us
 *
 * 执行过程: 使能→设方向→配置 GPTimer→启动→忙等→失能
 * 阻塞时长 ≈ steps × speed_us (如 1600 脉冲 × 1000us = 1.6s/圈)
 */
void step_rotate_angle(float angle, uint8_t dir, uint32_t speed_us)
{
    uint32_t steps = (uint32_t)((angle / 360.0f) * PULSE_PER_REV);
    if (steps == 0) return;

    step_enable();
    step_set_dir(dir);

    gptim_set_period(speed_us);
    gptim_set_pulse_count(steps);
    gptim_start();

    /* 忙等 GPTimer 发完所有脉冲 */
    while (gptim_is_running()) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    step_disable();
}

void step_rotate_turns(float turns, uint8_t dir, uint32_t speed_us)
{
    uint32_t steps = (uint32_t)(turns * PULSE_PER_REV);
    if (steps == 0) return;
    
    step_enable();
    step_set_dir(dir);
    
    gptim_set_period(speed_us);
    gptim_set_pulse_count(steps);
    gptim_start();
    
    while (gptim_is_running()) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    step_disable();
}

void step_stop(void)
{
    gptim_stop();
    step_disable();
}

// PWM 初始化实现
void motor_pwm_init(void)
{
    // 配置 LEDC 定时器
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_14_BIT, // 14位分辨率
        .freq_hz          = 50,                // 50Hz 频率 (ESC 常用)
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // 配置面粉电机 PWM 通道
    ledc_channel_config_t dough_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = DOUGH_LEDC_CH,
        .timer_sel      = LEDC_TIMER_D_M,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = dough_pwm,
        .duty           = 819, // 初始 5% 占空比 (1ms)
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&dough_channel));

    // 配置搅拌电机 PWM 通道
    ledc_channel_config_t mixer_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = MIXER_LEDC_CH,
        .timer_sel      = LEDC_TIMER_D_M,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = mixer_pwm,
        .duty           = 819, // 初始 5% 占空比 (1ms)
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&mixer_channel));

    //配置舵机定时器 (50Hz)
    ledc_timer_config_t servo_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_S,
        .duty_resolution = LEDC_RES_S,
        .freq_hz = 50,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&servo_timer));

    //配置通道
    ledc_channel_config_t channels[] = {
        {.channel = CH_STEERING_SERVO, .duty = 0, .gpio_num = steering_servo_pwm, .speed_mode = LEDC_LOW_SPEED_MODE, .hpoint = 0, .timer_sel = LEDC_TIMER_S},
        {.channel = CH_PUSH_SERVO, .duty = 0, .gpio_num = push_servo_pwm, .speed_mode = LEDC_LOW_SPEED_MODE, .hpoint = 0, .timer_sel = LEDC_TIMER_S},
        {.channel = DOUGH_LEDC_CH, .duty = 0, .gpio_num = dough_pwm, .speed_mode = LEDC_LOW_SPEED_MODE, .hpoint = 0, .timer_sel = LEDC_TIMER_D_M},
        {.channel = MIXER_LEDC_CH, .duty = 0, .gpio_num = mixer_pwm, .speed_mode = LEDC_LOW_SPEED_MODE, .hpoint = 0, .timer_sel = LEDC_TIMER_D_M},
    };

    for (int i = 0; i < 4; i++) {
        ESP_ERROR_CHECK(ledc_channel_config(&channels[i]));
    }

    ESP_LOGI(TAG, "Motor PWM (LEDC) initialized at 50Hz, 14-bit resolution");
}

//面粉电机函数实现
void dough_esc_init_and_start(void)
{
    uint32_t duty_5 = 819;
    uint32_t duty_8 = 1311;

    ESP_LOGI(TAG, "Dough Motor ESC Initializing: Setting 5%% duty...");
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, DOUGH_LEDC_CH, duty_5));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, DOUGH_LEDC_CH));
    
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "Dough Motor ESC Started: Setting 8%% duty...");
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, DOUGH_LEDC_CH, duty_8));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, DOUGH_LEDC_CH));
}

void dough_esc_stop(void)
{
    uint32_t duty_5 = 819;
    ESP_LOGI(TAG, "Dough Motor ESC Stopping: Setting 5%% duty...");
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, DOUGH_LEDC_CH, duty_5));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, DOUGH_LEDC_CH));
}

void dough_esc_cycle_run(uint8_t cycle_times, uint32_t on_time_ms, uint32_t off_time_ms)
{
    for (uint8_t i = 0; i < cycle_times; i++)
    {
        ESP_LOGI(TAG, "Dough ESC Cycle %d: ON for %d ms", i + 1, on_time_ms);
        dough_esc_init_and_start();
        vTaskDelay(pdMS_TO_TICKS(on_time_ms));

        ESP_LOGI(TAG, "Dough ESC Cycle %d: OFF", i + 1);
        dough_esc_stop();

        // 只有不是最后一次循环，才执行停止延时
        if (i != cycle_times - 1)
        {
            vTaskDelay(pdMS_TO_TICKS(off_time_ms));
        }
    }
    ESP_LOGI(TAG, "Dough ESC Cycle finished");
}

//搅拌电机函数实现
void mixer_esc_init_and_start(void)
{
    uint32_t duty_5 = 819;
    uint32_t duty_9 = 1475;

    ESP_LOGI(TAG, "Mixer Motor ESC Initializing: Setting 5%% duty...");
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, MIXER_LEDC_CH, duty_5));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, MIXER_LEDC_CH));
    
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "Mixer Motor ESC Started: Setting 9%% duty...");
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, MIXER_LEDC_CH, duty_9));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, MIXER_LEDC_CH));
}

void mixer_esc_stop(void)
{
    uint32_t duty_5 = 819;
    ESP_LOGI(TAG, "Mixer Motor ESC Stopping: Setting 5%% duty...");
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, MIXER_LEDC_CH, duty_5));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, MIXER_LEDC_CH));
}

void mixer_work(uint32_t work_times_min)
{
    mixer_esc_init_and_start();
    vTaskDelay(pdMS_TO_TICKS(work_times_min * 60 * 1000));
    mixer_esc_stop();
}

// 继电器初始化 (水泵和研磨电机)
void relay_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pump_pin) | (1ULL << grinder_pin) | (1ULL << magnet_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    
    // 初始状态关闭 (假设高电平触发或低电平触发，通常继电器常开端需要逻辑控制)
    // 这里统一设为 0，具体逻辑根据硬件继电器模块确定
    gpio_set_level(pump_pin, 0);
    gpio_set_level(grinder_pin, 0);
    gpio_set_level(magnet_pin, 0);
    ESP_LOGI(TAG, "Relay motors (Pump & Grinder & Magnet) initialized");
}

//水泵电机函数实现
void pump_start(void)
{
    gpio_set_level(pump_pin, 1);
    ESP_LOGI(TAG, "Pump started");
}

void pump_stop(void)
{
    gpio_set_level(pump_pin, 0);
    ESP_LOGI(TAG, "Pump stopped");
}

void pump_work(uint32_t pump_work_times_ms)
{
    pump_start();
    vTaskDelay(pdMS_TO_TICKS(pump_work_times_ms));
    pump_stop();
}

//研磨电机函数实现
void grinder_start(void)
{
    gpio_set_level(grinder_pin, 1);
    ESP_LOGI(TAG, "Grinder started");
}

void grinder_stop(void)
{
    gpio_set_level(grinder_pin, 0);
    ESP_LOGI(TAG, "Grinder stopped");
}

void grinder_work(uint32_t grinder_work_times_ms)
{
    grinder_start();
    vTaskDelay(pdMS_TO_TICKS(grinder_work_times_ms));
    grinder_stop();
}

//电磁铁函数实现
void magnet_start(void)
{
    gpio_set_level(magnet_pin, 1);
    ESP_LOGI(TAG, "Magnet started");
}

void magnet_stop(void)
{
    gpio_set_level(magnet_pin, 0);
    ESP_LOGI(TAG, "Magnet stopped");
}

void magnet_work(uint32_t magnet_work_times_ms)
{
    magnet_start();
    vTaskDelay(pdMS_TO_TICKS(magnet_work_times_ms));
    magnet_stop();
}

//舵机函数实现
void steering_servo_set_angle(float angle)
{
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;

    // 50Hz, 20ms. 0.5ms-2.5ms (500us-2500us) 对应 0-180度
    // 14-bit: 2^14 = 16384
    // 0.5ms: (0.5/20) * 16384 = 409.6
    // 2.5ms: (2.5/20) * 16384 = 2048
    uint32_t duty = (uint32_t)(409.6f + (angle / 180.0f) * (2048.0f - 409.6f));
    ledc_channel_t ch = CH_STEERING_SERVO;
    
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, ch));
}

void push_servo_set_angle(float angle)
{
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;

    // 50Hz, 20ms. 0.5ms-2.5ms (500us-2500us) 对应 0-180度
    // 14-bit: 2^14 = 16384
    // 0.5ms: (0.5/20) * 16384 = 409.6
    // 2.5ms: (2.5/20) * 16384 = 2048
    uint32_t duty = (uint32_t)(409.6f + (angle / 180.0f) * (2048.0f - 409.6f));
    ledc_channel_t ch = CH_PUSH_SERVO;
    
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, ch));
}

void servo_coordinated_control(void)
{
    /*
    // 1. 初始状态：两个舵机都在 0°
    steering_servo_set_angle(0.0f);
    push_servo_set_angle(0.0f);
    vTaskDelay(pdMS_TO_TICKS(500)); // 等待舵机到位
    */

    // 2. 流程第1步：push 转 90° → 立刻复位
    ESP_LOGI(TAG, "Servo Step 1: Push to 90° then reset");
    push_servo_set_angle(90.0f);
    vTaskDelay(pdMS_TO_TICKS(300));  // 给舵机运动时间
    push_servo_set_angle(0.0f);
    vTaskDelay(pdMS_TO_TICKS(300));

    // 3. 流程第2步：steering 转 90°
    ESP_LOGI(TAG, "Servo Step 2: Steering to 90°");
    steering_servo_set_angle(90.0f);
    vTaskDelay(pdMS_TO_TICKS(500));

    // 4. 流程第3步：push 再转 90° → 立刻复位
    ESP_LOGI(TAG, "Servo Step 3: Push to 90° then reset");
    push_servo_set_angle(90.0f);
    vTaskDelay(pdMS_TO_TICKS(300));
    push_servo_set_angle(0.0f);
    vTaskDelay(pdMS_TO_TICKS(300));

    // 5. 流程第4步：steering 再转 90°（同一方向，总共转180°）
    ESP_LOGI(TAG, "Servo Step 4: Steering to 180°");
    steering_servo_set_angle(180.0f);
    vTaskDelay(pdMS_TO_TICKS(500));

    // 6. 流程第5步：push 最后一次转 90° → 立刻复位
    ESP_LOGI(TAG, "Servo Step 5: Push to 90° then reset");
    push_servo_set_angle(90.0f);
    vTaskDelay(pdMS_TO_TICKS(300));
    push_servo_set_angle(0.0f);
    vTaskDelay(pdMS_TO_TICKS(300));

    ESP_LOGI(TAG, "Servo coordinated control sequence finished");
}

// 电机协同函数实现
/**
 * @brief 电机协同控制 (默认参数, 不传重量)
 *
 * 执行顺序 (7 步流水线):
 *   1. 面粉电机循环运行 (5 周期 × 10s ON + 5s OFF)
 *   2. 水泵 10s
 *   3. 研磨电机 10s
 *   4. 舵机协同动作
 *   5. 步进电机正转 1 圈
 *   6. 搅拌电机 1 分钟
 *   7. 步进电机反转 1 圈
 *
 * 每一步前检查 g_emergency_stop 标记, 置位则立即返回
 */
void motor_coordinated_control(void)
{
    motor_command_t cmd = {
        .cycle_times = 5,
        .on_time_ms = 10000,
        .off_time_ms = 5000,
        .pump_work_times_ms = 10000,
        .grinder_work_times_ms = 10000,
        .mixer_work_times_min = 1,
        .step_turns = 1.0f
    };

    g_emergency_stop = false;

    dough_esc_cycle_run(cmd.cycle_times, cmd.on_time_ms, cmd.off_time_ms);
    if (g_emergency_stop) return;
    pump_work(cmd.pump_work_times_ms);
    if (g_emergency_stop) return;
    grinder_work(cmd.grinder_work_times_ms);
    if (g_emergency_stop) return;
    servo_coordinated_control();
    if (g_emergency_stop) return;
    step_rotate_turns(cmd.step_turns, DIR_CW, 1000);
    if (g_emergency_stop) return;
    mixer_work(cmd.mixer_work_times_min);
    if (g_emergency_stop) return;
    step_rotate_turns(cmd.step_turns, DIR_CCW, 1000);
}

void motor_coordinated_control_ui_http(uint32_t weight)
{
    // 根据面粉重量计算各个电机的工作时间和步进电机的转数
    motor_command_t cmd = {
        .cycle_times = (weight + 49) / (DOUGH_G_PER_SEC * 10), // 粗略估算每10秒5克，向上取整
        .on_time_ms = 10000,
        .off_time_ms = 5000,
        .pump_work_times_ms = 10000,
        .grinder_work_times_ms = 10000,
        .mixer_work_times_min = 1,
        .step_turns = 1.0f
    };
    ESP_LOGI(TAG, "Coordinated (UI/HTTP): cycles=%u, pump=%lums, grinder=%lums, mixer=%lumin, step=%.1f turns",
             cmd.cycle_times,
             (unsigned long)cmd.pump_work_times_ms,
             (unsigned long)cmd.grinder_work_times_ms,
             (unsigned long)cmd.mixer_work_times_min,
             (double)cmd.step_turns);

    g_emergency_stop = false;

    dough_esc_cycle_run(cmd.cycle_times, cmd.on_time_ms, cmd.off_time_ms);
    if (g_emergency_stop) return;
    pump_work(cmd.pump_work_times_ms);
    if (g_emergency_stop) return;
    grinder_work(cmd.grinder_work_times_ms);
    if (g_emergency_stop) return;
    servo_coordinated_control();
    if (g_emergency_stop) return;
    step_rotate_turns(cmd.step_turns, DIR_CW, 1000);
    if (g_emergency_stop) return;
    mixer_work(cmd.mixer_work_times_min);
    if (g_emergency_stop) return;
    step_rotate_turns(cmd.step_turns, DIR_CCW, 1000);
}


