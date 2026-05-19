#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "motor.h"
#include "uart.h"
#include "wifi.h"
#include "http.h"
#include "spi.h"
#include "iic.h"
#include "lvgl_ui.h"
#include "esp_log.h"

void app_main(void)
{
    /* SPI 总线初始化 (LCD 通信) */
    spi_lcd_init();

    /* I2C 总线初始化 (触摸通信) */
    i2c_master_bus_handle_t i2c_bus = NULL;
    iic_touch_init(&i2c_bus);

    /* LVGL 初始化 (LCD + 触摸 + UI) */
    lvgl_ui_init(i2c_bus);

    /* 屏幕测试 */
    lvgl_test_screen();

    /* 初始化电机 */
    step_init();

    /* 初始化串口 */
    uart_init();

    /* 初始化 WiFi AP */
    wifi_init_softap();

    /* 启动串口监听任务 */
    xTaskCreate(uart_command_task, "uart1_cmd", 4096, NULL, 10, NULL);
    xTaskCreate(uart0_command_task, "uart0_cmd", 4096, NULL, 10, NULL);

    /* 启动 TCP 服务器任务 */
    xTaskCreate(tcp_server_task, "tcp_server", 4096, (void*)AF_INET, 5, NULL);

    /* 启动 HTTP 服务器 */
    http_server_start();

    ESP_LOGI("MAIN", "System Started, WiFi SSID: dough_mixer, PWD: 12345678");
    ESP_LOGI("MAIN", "TCP Server listening on port 8080");
    ESP_LOGI("MAIN", "HTTP Server listening on port 80");

    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
