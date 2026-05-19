#ifndef __LCD_H
#define __LCD_H

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化 ST7789V LCD 面板，返回 IO 和面板句柄 */
void lcd_init(esp_lcd_panel_io_handle_t *ret_io, esp_lcd_panel_handle_t *ret_panel);

#ifdef __cplusplus
}
#endif

#endif
