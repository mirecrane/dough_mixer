#ifndef __LVGL_UI_H__
#define __LVGL_UI_H__

#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/* LVGL 初始化（含 LCD 和触摸） */
void lvgl_ui_init(i2c_master_bus_handle_t i2c_bus);

/* 屏幕测试：显示三个字体的文字 + 圆弧控件 */
void lvgl_test_screen(void);

#ifdef __cplusplus
}
#endif

#endif // __LVGL_UI_H__
