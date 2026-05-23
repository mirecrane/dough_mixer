#ifndef __GPTIM_H__
#define __GPTIM_H__

#include <stdint.h>
#include "driver/gptimer.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DIR_CW = 0,
    DIR_CCW = 1
} motor_dir_t;

void gptim_init(gpio_num_t pin);
void gptim_set_period(uint32_t period_us);
void gptim_start(void);
void gptim_stop(void);
void gptim_set_pulse_count(uint32_t count);
bool gptim_is_running(void);

#ifdef __cplusplus
}
#endif

#endif
