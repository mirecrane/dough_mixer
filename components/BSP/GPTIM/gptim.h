/**
 * @file    gptim.h
 * @brief   通用定时器模块 — 为步进电机产生精确脉冲序列
 *
 * 基于 ESP-IDF gptimer 硬件定时器, 1MHz 分辨率.
 * 每个 alarm 事件翻转一次 GPIO → 产生方波脉冲.
 * 通过 pulse_count 计数实现精确步数控制.
 */

#ifndef __GPTIM_H__
#define __GPTIM_H__

#include <stdint.h>
#include "driver/gptimer.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 步进电机方向枚举 */
typedef enum {
    DIR_CW  = 0,  /**< 顺时针 (正向) */
    DIR_CCW = 1   /**< 逆时针 (反向) */
} motor_dir_t;

/** 初始化 GPTimer, 配置 1MHz 计数 + auto-reload alarm + GPIO 输出 */
void gptim_init(gpio_num_t pin);

/** 设置脉冲周期 (us), 实际 alarm 周期 = period_us/2 (半周期翻转一次) */
void gptim_set_period(uint32_t period_us);

/** 启动脉冲输出 (需先设 period + pulse_count) */
void gptim_start(void);

/** 停止脉冲输出并禁用定时器 */
void gptim_stop(void);

/** 设置目标脉冲数 (上升沿计数) */
void gptim_set_pulse_count(uint32_t count);

/** 查询是否正在输出脉冲 */
bool gptim_is_running(void);

#ifdef __cplusplus
}
#endif

#endif
