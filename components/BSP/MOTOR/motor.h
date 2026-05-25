#ifndef __MOTOR_H__
#define __MOTOR_H__

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "gptim.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//步进电机引脚定义
#define step_stp  GPIO_NUM_4
#define step_dir  GPIO_NUM_5
#define step_en   GPIO_NUM_6

//面粉电机引脚定义
#define dough_pwm GPIO_NUM_47//GPIO_NUM_15

//搅拌电机引脚定义
#define mixer_pwm GPIO_NUM_48//GPIO_NUM_9

//水泵电机引脚定义
#define pump_pin GPIO_NUM_7

//研磨电机引脚定义
#define grinder_pin GPIO_NUM_16

//舵机引脚定义
#define steering_servo_pwm GPIO_NUM_8
#define push_servo_pwm GPIO_NUM_3

//步进电机参数
#define MICROSTEPS      8
#define BASE_STEPS      200
#define PULSE_PER_REV   (BASE_STEPS * MICROSTEPS)
#define ANGLE_PER_PULSE (360.0f / PULSE_PER_REV)

//定时器参数
#define LEDC_TIMER_D_M LEDC_TIMER_0
#define LEDC_TIMER_S   LEDC_TIMER_1

#define LEDC_RES_S   LEDC_TIMER_14_BIT

//面粉电机参数
#define DOUGH_LEDC_CH LEDC_CHANNEL_0
#define DOUGH_G_PER_SEC 5.0f // 每秒5克，实际根据电机和负载调整

//搅拌电机参数
#define MIXER_LEDC_CH LEDC_CHANNEL_1

//水泵电机参数
#define PUMP_ML_PER_SEC 10.0f // 每秒10毫升，实际根据泵的规格调整

//舵机参数
#define CH_STEERING_SERVO   LEDC_CHANNEL_0
#define CH_PUSH_SERVO   LEDC_CHANNEL_1


//步进电机函数
void step_init(void);
void step_set_dir(uint8_t dir);
void step_enable(void);
void step_disable(void);
void step_rotate_angle(float angle, uint8_t dir, uint32_t speed_us);
void step_rotate_turns(float turns, uint8_t dir, uint32_t speed_us);
void step_stop(void);

// PWM 初始化
void motor_pwm_init(void);

// 继电器初始化
void relay_init(void);

//面粉电机函数
void dough_esc_init_and_start(void);
void dough_esc_stop(void);
void dough_esc_cycle_run(uint8_t cycle_times, uint32_t on_time_ms, uint32_t off_time_ms);

//搅拌电机函数
void mixer_esc_init_and_start(void);
void mixer_esc_stop(void);
void mixer_work(uint32_t work_times_min);

//水泵电机函数
void pump_start(void);
void pump_stop(void);
void pump_work(uint32_t pump_work_times_ms);

//研磨电机函数
void grinder_start(void);   
void grinder_stop(void);
void grinder_work(uint32_t grinder_work_times_ms);

//舵机函数
void steering_servo_set_angle(float angle);
void push_servo_set_angle(float angle);
void servo_coordinated_control(void);

// 步进电机往返模式
void step_start_auto_mode(void);
void step_stop_auto_mode(void);

// 紧急停止
void motor_emergency_stop(void);

// 电机协同函数

//参数结构体
typedef struct {
    uint8_t cycle_times;
    uint32_t on_time_ms; 
    uint32_t off_time_ms;
    uint32_t pump_work_times_ms;
    uint32_t grinder_work_times_ms;
    uint32_t mixer_work_times_min;
    float step_turns;
} motor_command_t;

void motor_coordinated_control(void);

void motor_coordinated_control_ui_http(uint32_t weight);

#ifdef __cplusplus
}
#endif

#endif // __MOTOR_H__
