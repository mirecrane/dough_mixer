#include "uart.h"
#include "motor.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "UART";
static char line_buf_1[UART_BUF_SIZE];
static int line_pos_1 = 0;
static char line_buf_0[UART_BUF_SIZE];
static int line_pos_0 = 0;

static void process_cmd(uart_port_t uart_num, char *str);
static TaskHandle_t timed_task_handle = NULL;

static void uart_send_msg(uart_port_t uart_num, const char *msg)
{
    uart_write_bytes(uart_num, msg, strlen(msg));
    uart_write_bytes(uart_num, "\r\n", 2);
}

typedef struct {
    char type; // 'P', 'D', 'M', 'G'
    int val;   // seconds or minutes
    uart_port_t uart_num;
} timed_cmd_t;

static void timed_action_task(void *pvParameters)
{
    timed_cmd_t *cmd = (timed_cmd_t *)pvParameters;
    char msg[64];
    
    if (cmd->type == 'P') {
        snprintf(msg, sizeof(msg), "Pump starting for %d seconds...", cmd->val);
        uart_send_msg(cmd->uart_num, msg);
        pump_start();
        vTaskDelay(pdMS_TO_TICKS(cmd->val * 1000));
        pump_stop();
        uart_send_msg(cmd->uart_num, "Pump stopped.");
    } else if (cmd->type == 'D') {
        snprintf(msg, sizeof(msg), "Dough motor starting for %d seconds...", cmd->val);
        uart_send_msg(cmd->uart_num, msg);
        dough_esc_init_and_start();
        vTaskDelay(pdMS_TO_TICKS(cmd->val * 1000));
        dough_esc_stop();
        uart_send_msg(cmd->uart_num, "Dough motor stopped.");
    } else if (cmd->type == 'M') { // Mixer
        snprintf(msg, sizeof(msg), "Mixer motor starting for %d minutes...", cmd->val);
        uart_send_msg(cmd->uart_num, msg);
        mixer_esc_init_and_start();
        vTaskDelay(pdMS_TO_TICKS(cmd->val * 60 * 1000));
        mixer_esc_stop();
        uart_send_msg(cmd->uart_num, "Mixer motor stopped.");
    } else if (cmd->type == 'G') {
        snprintf(msg, sizeof(msg), "Grinder starting for %d seconds...", cmd->val);
        uart_send_msg(cmd->uart_num, msg);
        grinder_start();
        vTaskDelay(pdMS_TO_TICKS(cmd->val * 1000));
        grinder_stop();
        uart_send_msg(cmd->uart_num, "Grinder stopped.");
    }
    
    free(cmd);
    timed_task_handle = NULL;
    vTaskDelete(NULL);
}

static void start_timed_task(uart_port_t uart_num, char type, int val)
{
    // 如果已有任务在运行，先停止它（或者您可以选择忽略新指令，取决于需求）
    if (timed_task_handle != NULL) {
        vTaskDelete(timed_task_handle);
        motor_emergency_stop(); // 停止所有，确保安全
    }
    
    timed_cmd_t *cmd = malloc(sizeof(timed_cmd_t));
    cmd->type = type;
    cmd->val = val;
    cmd->uart_num = uart_num;
    xTaskCreate(timed_action_task, "timed_act", 4096, cmd, 5, &timed_task_handle);
}

/**
 * @brief 初始化串口
 */
void uart_init(void)
{
    // 初始化 UART1
    uart_config_t uart_config_1 = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config_1));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // 初始化 UART0 (Console) - 通常已经初始化，但这里确保驱动安装
    if (!uart_is_driver_installed(UART_NUM_0)) {
        uart_config_t uart_config_0 = {
            .baud_rate = 115200,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .source_clk = UART_SCLK_DEFAULT,
        };
        ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, UART_BUF_SIZE * 2, 0, 0, NULL, 0));
        ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart_config_0));
    }
}

/**
 * @brief 串口指令监听任务 (UART1)
 */
void uart_command_task(void *pvParameters)
{
    uint8_t ch;
    line_pos_1 = 0;
    
    while (1) {
        if (uart_read_bytes(UART_NUM, &ch, 1, portMAX_DELAY) > 0) {
            if (ch == '\n' || ch == '\r') {
                line_buf_1[line_pos_1] = '\0';
                if (line_pos_1 > 0) {
                    process_cmd(UART_NUM, line_buf_1);
                }
                line_pos_1 = 0;
            } else {
                if (line_pos_1 < UART_BUF_SIZE - 1) {
                    line_buf_1[line_pos_1++] = ch;
                }
            }
        }
    }
}

/**
 * @brief 串口指令监听任务 (UART0/Console)
 */
void uart0_command_task(void *pvParameters)
{
    uint8_t ch;
    line_pos_0 = 0;
    
    while (1) {
        if (uart_read_bytes(UART_NUM_0, &ch, 1, portMAX_DELAY) > 0) {
            if (ch == '\n' || ch == '\r') {
                line_buf_0[line_pos_0] = '\0';
                if (line_pos_0 > 0) {
                    process_cmd(UART_NUM_0, line_buf_0);
                }
                line_pos_0 = 0;
            } else {
                if (line_pos_0 < UART_BUF_SIZE - 1) {
                    line_buf_0[line_pos_0++] = ch;
                }
            }
        }
    }
}

/**
 * @brief 处理解析后的指令
 */
static void process_cmd(uart_port_t uart_num, char *str)
{
    // 去除开头空格
    while(*str == ' ') str++;
    
    // 去除结尾空格和不可见字符
    int len = strlen(str);
    while(len > 0 && (str[len-1] == ' ' || str[len-1] == '\r' || str[len-1] == '\n')) {
        str[--len] = '\0';
    }

    if (len == 0) return;

    ESP_LOGI(TAG, "Executing command: [%s]", str);

    char msg[64];

    // 1. 全局停止
    if (strcasecmp(str, "STOP") == 0) {
        if (timed_task_handle != NULL) {
            vTaskDelete(timed_task_handle);
            timed_task_handle = NULL;
        }
        motor_emergency_stop();
        uart_send_msg(uart_num, "Emergency Stop All: SUCCESS");
        return;
    }

    // 2. 转向舵机 SS A <角度>
    if (strncasecmp(str, "SS A ", 5) == 0) {
        float angle = atof(str + 5);
        steering_servo_set_angle(angle);
        snprintf(msg, sizeof(msg), "Steering servo set to %.1f deg: SUCCESS", angle);
        uart_send_msg(uart_num, msg);
        return;
    }

    // 3. 推动舵机 SP A <角度>
    if (strncasecmp(str, "SP A ", 5) == 0) {
        float angle = atof(str + 5);
        push_servo_set_angle(angle);
        snprintf(msg, sizeof(msg), "Push servo set to %.1f deg: SUCCESS", angle);
        uart_send_msg(uart_num, msg);
        return;
    }

    // 4. 步进电机
    if (strncasecmp(str, "M ", 2) == 0) {
        char *sub = str + 2;
        if (strncasecmp(sub, "A ", 2) == 0) {
            float angle = atof(sub + 2);
            step_rotate_angle(angle, DIR_CW, 1000);
            snprintf(msg, sizeof(msg), "Stepper rotated %.1f deg: SUCCESS", angle);
            uart_send_msg(uart_num, msg);
        } else if (strcasecmp(sub, "START") == 0) {
            step_start_auto_mode();
            uart_send_msg(uart_num, "Stepper auto mode started: SUCCESS");
        } else if (strncasecmp(sub, "B ", 2) == 0) {
            float angle = atof(sub + 2);
            step_rotate_angle(angle, DIR_CCW, 1000);
            snprintf(msg, sizeof(msg), "Stepper rotated -%.1f deg: SUCCESS", angle);
            uart_send_msg(uart_num, msg);
        }else if (strcasecmp(sub, "STOP") == 0) {
            step_stop_auto_mode();
            uart_send_msg(uart_num, "Stepper auto mode stopped: SUCCESS");
        } else if (strncasecmp(sub, "F ", 2) == 0) {
            float turns = atof(sub + 2);
            step_rotate_turns(turns, DIR_CW, 1000);
            snprintf(msg, sizeof(msg), "Stepper rotated %.1f turns: SUCCESS", turns);
            uart_send_msg(uart_num, msg);
        } else if (strncasecmp(sub, "R ", 2) == 0) {
            float turns = atof(sub + 2);
            step_rotate_turns(turns, DIR_CCW, 1000);
            snprintf(msg, sizeof(msg), "Stepper rotated -%.1f turns: SUCCESS", turns);
            uart_send_msg(uart_num, msg);
        }
        return;
    }

    // 5. 水泵 P <时间> (秒)
    if (strncasecmp(str, "P ", 2) == 0) {
        int sec = atoi(str + 2);
        start_timed_task(uart_num, 'P', sec);
        uart_send_msg(uart_num, "Pump command received: SUCCESS");
        return;
    }

    // 6. 面粉电机 D <时间> (秒)
    if (strncasecmp(str, "D ", 2) == 0) {
        int sec = atoi(str + 2);
        start_timed_task(uart_num, 'D', sec);
        uart_send_msg(uart_num, "Dough motor command received: SUCCESS");
        return;
    }

    // 7. 搅拌电机 MI <时间> (分钟)
    if (strncasecmp(str, "MI ", 3) == 0) {
        int min = atoi(str + 3);
        start_timed_task(uart_num, 'M', min);
        uart_send_msg(uart_num, "Mixer command received: SUCCESS");
        return;
    }

    // 8. 研磨电机 G <时间> (秒)
    if (strncasecmp(str, "G ", 2) == 0) {
        int sec = atoi(str + 2);
        start_timed_task(uart_num, 'G', sec);
        uart_send_msg(uart_num, "Grinder command received: SUCCESS");
        return;
    }

    // 9. 协同控制 huomian
    if (strcasecmp(str, "H") == 0) {
        motor_coordinated_control();
        uart_send_msg(uart_num, "Coordinated control started: SUCCESS");
        return;
    }

    uart_send_msg(uart_num, "Error: Unknown command");
}