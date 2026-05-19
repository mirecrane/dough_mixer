#ifndef __FT6336U_TOUCH_H__
#define __FT6336U_TOUCH_H__

#include "driver/i2c_master.h"
#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ST7789V 屏幕分辨率 */
#define LCD_H_RES       240
#define LCD_V_RES       320

/* 初始化 FT6336U 触摸控制器 */
void touch_init(i2c_master_bus_handle_t i2c_bus);

/* 获取触摸句柄 */
esp_lcd_touch_handle_t touch_get_handle(void);

#ifdef __cplusplus
}
#endif

#endif // __FT6336U_TOUCH_H__
