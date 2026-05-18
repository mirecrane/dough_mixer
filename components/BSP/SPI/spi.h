#ifndef __SPI_H
#define __SPI_H

#include <string.h>
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

// 屏幕 SPI 接口引脚（按转接板顺序排列）

#define LCD_CS          GPIO_NUM_10    // SPI 片选引脚，低电平选中屏幕
#define LCD_RST         GPIO_NUM_11    // 屏幕硬件复位引脚，低电平复位
#define LCD_DC          GPIO_NUM_12    // 数据/命令切换引脚，高=数据，低=命令
#define LCD_MOSI        GPIO_NUM_13    // SPI 主机发从机收数据引脚（SDI）
#define LCD_SCK         GPIO_NUM_14    // SPI 时钟信号引脚
#define LCD_BL          GPIO_NUM_17    // 屏幕背光控制引脚，高电平点亮背光
#define LCD_MISO        GPIO_NUM_18    // SPI 从机发主机收数据引脚（SDO）

/* 初始化 SPI2 总线 (LCD 通信) */
void spi_lcd_init(void);

#ifdef __cplusplus
}
#endif

#endif
