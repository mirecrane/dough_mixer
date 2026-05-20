#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lcd.h"
#include "touch.h"
#include "lvgl_ui.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    // 1. 屏幕初始化
    bsp_lcd_init();
    ESP_LOGI(TAG, "LCD 初始化完成");

    // 2. 触摸初始化
    ft6336u_touch_init();

    // 3. LVGL 初始化（内部注册 LCD + 触摸）
    lvgl_ui_init();

    // 4. 创建测试界面
    lvgl_ui_test();

    ESP_LOGI(TAG, "初始化完成，LVGL 触摸测试运行中");
}
