/**
 * @file    motor.h
 * @brief   和面机全电机控制模块
 *
 * 管理 6 类执行器:
 *   1. 步进电机   — 48mm 两相四线, A4988 驱动, 8 微步
 *   2. 面粉电机   — 无刷电机 + 30A ESC, 50Hz PWM (5%~8% duty)
 *   3. 搅拌电机   — 无刷电机 + 30A ESC, 50Hz PWM (5%~9% duty)
 *   4. 水泵       — 12V 直流泵, 继电器控制
 *   5. 研磨电机   — 继电器控制
 *   6. 舵机 × 2  — 50Hz PWM, 转向舵机 + 推动舵机
 */

#ifndef __MOTOR_H__
#define __MOTOR_H__

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "gptim.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*====================================================================
 *  引脚定义
 *====================================================================*/

/* 步进电机 — A4988 驱动模块 */
#define step_stp  GPIO_NUM_4   /**< 脉冲信号, 上升沿触发一步 */
#define step_dir  GPIO_NUM_5   /**< 方向信号, 0=CW 顺时针, 1=CCW 逆时针 */
#define step_en   GPIO_NUM_6   /**< 使能信号, 1=脱机(自由), 0=锁定 */

/* 面粉电机 — 无刷 ESC PWM */
#define dough_pwm GPIO_NUM_15  /**< 面粉电机 PWM 信号线 (原 GPIO15 有冲突) */

/* 搅拌电机 — 无刷 ESC PWM */
#define mixer_pwm GPIO_NUM_9  /**< 搅拌电机 PWM 信号线 (原 GPIO9 有冲突) */

/* 水泵 / 研磨电机 — 继电器 */
#define pump_pin    GPIO_NUM_7   /**< 水泵继电器控制引脚 */
#define grinder_pin GPIO_NUM_16  /**< 研磨电机继电器控制引脚 */
#define magnet_pin  GPIO_NUM_18  /**< 电磁铁继电器控制引脚 */



/* 舵机 — 50Hz PWM */
#define steering_servo_pwm GPIO_NUM_8  /**< 转向舵机 */
#define push_servo_pwm     GPIO_NUM_3  /**< 推动舵机 */

/*====================================================================
 *  步进电机物理参数
 *====================================================================*/
#define MICROSTEPS      8               /**< A4988 微步细分: 8 微步 */
#define BASE_STEPS      200             /**< 电机固有步数/圈: 200 (1.8°/步) */
#define PULSE_PER_REV   (BASE_STEPS * MICROSTEPS)  /**< 实际脉冲/圈: 1600 */
#define ANGLE_PER_PULSE (360.0f / PULSE_PER_REV)   /**< 每脉冲角度: 0.225° */

/*====================================================================
 *  LEDC (PWM) 定时器 & 通道分配
 *====================================================================*/
#define LEDC_TIMER_D_M  LEDC_TIMER_0    /**< 面粉+搅拌 ESC 共用定时器 */
#define LEDC_TIMER_S    LEDC_TIMER_1    /**< 舵机专用定时器 */
#define LEDC_RES_S      LEDC_TIMER_14_BIT /**< 舵机 PWM 分辨率: 14 位 */

/* 面粉电机 PWM */
#define DOUGH_LEDC_CH   LEDC_CHANNEL_0  /**< 面粉 ESC LEDC 通道 */
#define DOUGH_G_PER_SEC 5.0f            /**< 面粉出粉速度: 5 克/秒 (可调) */

/* 搅拌电机 PWM */
#define MIXER_LEDC_CH   LEDC_CHANNEL_1  /**< 搅拌 ESC LEDC 通道 */

/* 水泵参数 */
#define PUMP_ML_PER_SEC 10.0f           /**< 水泵流量: 10 毫升/秒 (可调) */

/* 舵机 PWM 通道 */
#define CH_STEERING_SERVO  LEDC_CHANNEL_0 /**< 转向舵机通道 (timer S) */
#define CH_PUSH_SERVO      LEDC_CHANNEL_1 /**< 推动舵机通道 (timer S) */

/*====================================================================
 *  步进电机 API
 *====================================================================*/
void step_init(void);       /**< 初始化 GPIO + GPTimer + PWM + 继电器 */
void step_set_dir(uint8_t dir);  /**< 设置方向, DIR_CW 或 DIR_CCW */
void step_enable(void);     /**< 使能 (锁定转子) */
void step_disable(void);    /**< 失能 (脱机自由) */
void step_rotate_angle(float angle, uint8_t dir, uint32_t speed_us);
void step_rotate_turns(float turns, uint8_t dir, uint32_t speed_us);
void step_stop(void);       /**< 立即停止脉冲输出 */

void motor_pwm_init(void);  /**< 初始化所有 ESC PWM 和舵机 PWM */
void relay_init(void);      /**< 初始化水泵和研磨继电器 GPIO */

/*====================================================================
 *  面粉电机 API (无刷 ESC)
 *====================================================================*/
void dough_esc_init(void);            /**< ESC 校准 (5% duty) */
void dough_esc_init_and_start(void);  /**< ESC 校准 + 启动 */
void dough_esc_stop(void);            /**< ESC 停止 */
void dough_esc_cycle_run(uint8_t cycle_times, uint32_t on_time_ms,
                         uint32_t off_time_ms);  /**< 循环运行多个 ON-OFF 周期 */

// 电磁铁

/*====================================================================
 *  搅拌电机 API (无刷 ESC)
 *====================================================================*/
void mixer_esc_init_and_start(void);
void mixer_esc_stop(void);
void mixer_work(uint32_t work_times_min);  /**< 运行指定分钟数后自动停止 */

/*====================================================================
 *  水泵 API (继电器)
 *====================================================================*/
void pump_start(void);
void pump_stop(void);
void pump_work(uint32_t pump_work_times_ms);  /**< 运行指定毫秒数后自动停止 */

/*====================================================================
 *  研磨电机 API (继电器)
 *====================================================================*/
void grinder_start(void);
void grinder_stop(void);
void grinder_work(uint32_t grinder_work_times_ms);

/*====================================================================
 *  电磁铁 API
 *====================================================================*/
void magnet_start(void);
void magnet_stop(void);
void magnet_work(uint32_t magnet_work_times_ms);

/*====================================================================
 *  舵机 API
 *====================================================================*/
void steering_servo_set_angle(float angle);  /**< 0°~180° */
void push_servo_set_angle(float angle);      /**< 0°~180° */
void servo_coordinated_control(void);        /**< 舵机协同动作序列 */

/*====================================================================
 *  步进电机自动模式
 *====================================================================*/
void step_start_auto_mode(void);  /**< FreeRTOS 任务: 正180°→停→反180° 循环 */
void step_stop_auto_mode(void);

/*====================================================================
 *  全局控制
 *====================================================================*/
void motor_emergency_stop(void);  /**< 紧急停止: 销毁所有任务 + 停止所有电机 */

/*====================================================================
 *  协同控制 (多电机顺序执行)
 *====================================================================*/

/** 协同控制参数 */
typedef struct {
    uint8_t  cycle_times;        /**< 面粉电机循环次数 */
    uint32_t on_time_ms;         /**< 面粉电机每次运行毫秒数 */
    uint32_t off_time_ms;        /**< 面粉电机间隔毫秒数 */
    uint32_t pump_work_times_ms; /**< 水泵运行毫秒数 */
    uint32_t grinder_work_times_ms; /**< 研磨电机运行毫秒数 */
    uint32_t mixer_work_times_min; /**< 搅拌电机运行分钟数 */
    float    step_turns;         /**< 步进电机转动圈数 */
} motor_command_t;

/** 使用默认参数启动协同控制 */
void motor_coordinated_control(void);

/**
 * @brief 根据面团重量启动协同控制
 * @param weight 面团重量 (克), 自动换算循环次数和搅拌时间
 *               循环数 = weight / 50, 搅拌固定 2 分钟
 */
void motor_coordinated_control_ui_http(uint32_t weight);

#ifdef __cplusplus
}
#endif

#endif /* __MOTOR_H__ */
