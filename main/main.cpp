#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "lcd.h"
#include "touch.h"
#include "lvgl_ui.h"
#include "ui.h"
#include "vars.h"
#include "motor.h"
#include "uart.h"
#include "wifi.h"
#include "http.h"

static const char *TAG = "MAIN";

extern "C" void app_main(void)
{
    /* ---- NVS / 系统基础 ---- */
    nvs_flash_init();
    esp_event_loop_create_default();

    /* ---- 电机 ---- */
    step_init();
    ESP_LOGI(TAG, "Motor init done");

    /* ---- 串口 ---- */
    uart_init();
    xTaskCreate(uart_command_task, "uart1_cmd", 4096, NULL, 10, NULL);
    xTaskCreate(uart0_command_task, "uart0_cmd", 4096, NULL, 10, NULL);
    ESP_LOGI(TAG, "UART init done");

    /* ---- WiFi AP ---- */
    wifi_init_softap();

    /* ---- TCP 服务器 (8080 调试) ---- */
    xTaskCreate(tcp_server_task, "tcp_server", 4096, (void*)AF_INET, 5, NULL);

    /* ---- HTTP 服务器 (80 网页) ---- */
    http_server_start();

    /* ---- LCD ---- */
    bsp_lcd_init();
    ESP_LOGI(TAG, "LCD init done");

    /* ---- 触摸 ---- */
    ft6336u_touch_init();

    /* ---- LVGL（注册 LCD + 触摸）---- */
    lvgl_ui_init();

    /* ---- EEZ Studio UI ---- */
    lvgl_port_lock(0);
    vars_init();
    ui_init();
    lvgl_port_unlock();

    ESP_LOGI(TAG, "System started, WiFi: dough_mixer / 12345678");

    while (1) {
        lvgl_port_lock(0);
        ui_tick();
        lvgl_port_unlock();

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
