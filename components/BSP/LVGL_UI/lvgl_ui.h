#ifndef __LVGL_UI_H__
#define __LVGL_UI_H__

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化LVGL（含LCD显示和触摸输入注册）
 */
void lvgl_ui_init(void);

/**
 * @brief 创建触摸坐标测试界面
 */
void lvgl_ui_test(void);

#ifdef __cplusplus
}
#endif

#endif // __LVGL_UI_H__
