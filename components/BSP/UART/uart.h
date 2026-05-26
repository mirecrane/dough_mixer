/**
 * @file    uart.h
 * @brief   串口指令控制模块
 *
 * 监听 UART0 (USB 串口) 和 UART1, 接收文本指令控制电机.
 * 指令格式与 TCP (WiFi) 完全一致, 大小写不敏感.
 * 每条指令以回车结束, 新指令到达时会停止上一个定时任务.
 */

#ifndef __UART_H__
#define __UART_H__

#include "driver/uart.h"

#ifdef __cplusplus
extern "C" {
#endif

/** UART1 端口号 */
#define UART_NUM            UART_NUM_1

/** UART 收发引脚 */
#define UART_TX_PIN         GPIO_NUM_17  /**< 注意: 与 LCD_BL 历史冲突, 现已改用 GPIO1 */
#define UART_RX_PIN         GPIO_NUM_18

/** 接收缓冲区大小 (字节) */
#define UART_BUF_SIZE       1024

/** 初始化 UART0 + UART1 (115200bps 8N1) */
void uart_init(void);

/** UART1 指令监听任务 */
void uart_command_task(void *pvParameters);

/** UART0 (Console) 指令监听任务 */
void uart0_command_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif
