/**
 * @file    gptim.c
 * @brief   GPTimer 脉冲发生器 — 步进电机底层驱动.
 *
 * 工作原理: 1MHz 计数, alarm ISR 翻转 GPIO, 下降沿计数, 达标自动停.
 * 状态机:   INIT → enable → READY → start → RUNNING → stop → disable → INIT
 * 每次 gptim_start() 先 stop+disable 确保从 INIT 态安全启动.
 */
#include "gptim.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "GPTIM";

static gptimer_handle_t gptimer = NULL;
static volatile bool is_running = false;          /**< 脉冲输出进行中 */
static volatile uint32_t pulse_count = 0;         /**< 已输出脉冲数 (下降沿计数) */
static volatile uint32_t target_pulse_count = 0;
static gpio_num_t step_pin;

static bool gptimer_alarm_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    static uint8_t level = 0;
    if (is_running) {
        if (pulse_count < target_pulse_count) {
            level = !level;
            gpio_set_level(step_pin, level);
            if (level == 0) {
                pulse_count++;
            }
        } else {
            gpio_set_level(step_pin, 0);
            level = 0;
            is_running = false;
        }
    }
    return false;
}

void gptim_init(gpio_num_t pin)
{
    step_pin = pin;
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

    gptimer_alarm_config_t alarm_config = {
        .alarm_count = 1000,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));

    gptimer_event_callbacks_t cbs = {
        .on_alarm = gptimer_alarm_cb,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, NULL));

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << step_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    
    ESP_LOGI(TAG, "GPTIM initialized");
}

void gptim_set_period(uint32_t period_us)
{
    if (gptimer) {
        gptimer_alarm_config_t alarm_config = {
            .alarm_count = period_us / 2,
            .reload_count = 0,
            .flags.auto_reload_on_alarm = true,
        };
        gptimer_set_alarm_action(gptimer, &alarm_config);
    }
}

void gptim_start(void)
{
    if (gptimer && !is_running) {
        /* 确保 timer 回到 INIT 态再 enable，避免 ISR 完成后残留 READY 态 */
        gptimer_stop(gptimer);
        gptimer_disable(gptimer);
        pulse_count = 0;
        is_running = true;
        gptimer_enable(gptimer);
        gptimer_start(gptimer);
    }
}

void gptim_stop(void)
{
    if (gptimer) {
        gptimer_stop(gptimer);
        gptimer_disable(gptimer);
        is_running = false;
    }
}

void gptim_set_pulse_count(uint32_t count)
{
    target_pulse_count = count;
}

bool gptim_is_running(void)
{
    return is_running;
}
