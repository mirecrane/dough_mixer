#ifndef __IIC_H
#define __IIC_H

#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

// 电容触摸 CTP 接口引脚（I2C 通信 + 控制）

#define CTP_SCL         GPIO_NUM_21    // 电容触摸 I2C 时钟引脚
#define CTP_SDA         GPIO_NUM_40    // 电容触摸 I2C 数据引脚
#define CTP_INT         GPIO_NUM_41    // 电容触摸中断引脚，有触摸时触发
#define CTP_RST         GPIO_NUM_42    // 电容触摸芯片复位引脚，低电平复位

/**
 * @brief 初始化 I2C0 总线，用于触摸控制器 FT6336U 
 * @param ret_bus  输出：I2C 总线句柄
 */
void iic_touch_init(i2c_master_bus_handle_t *ret_bus);

#ifdef __cplusplus
}
#endif

#endif
