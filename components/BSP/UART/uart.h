#ifndef __UART_H__
#define __UART_H__

#include "driver/uart.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UART_NUM            UART_NUM_1
#define UART_TX_PIN         GPIO_NUM_17
#define UART_RX_PIN         GPIO_NUM_18
#define UART_BUF_SIZE       1024

// 初始化串口
void uart_init(void);

//void process_cmd(char *str);

// 串口指令解析任务
void uart_command_task(void *pvParameters);
void uart0_command_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif