#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "lcd.h"
#include "touch.h"
#include "lvgl_ui.h"
#include "ui.h"
#include "vars.h"

static const char *TAG = "MAIN";

extern "C" void app_main(void)
{
    /* 1. LCD 硬件初始化 */
    bsp_lcd_init();
    ESP_LOGI(TAG, "LCD init done");

    /* 2. 触摸初始化 */
    ft6336u_touch_init();

    /* 3. LVGL 初始化（注册 LCD + 触摸） */
    lvgl_ui_init();

    /* 4. EEZ Studio UI 显示 */
    lvgl_port_lock(0);
    vars_init();
    ui_init();
    lvgl_port_unlock();

    ESP_LOGI(TAG, "System started, EEZ UI running");

    while (1) {
        lvgl_port_lock(0);
        ui_tick();
        lvgl_port_unlock();

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
