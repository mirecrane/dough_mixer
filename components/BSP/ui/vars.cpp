/**
 * @file    vars.cpp
 * @brief   EEZ UI 全局变量实现 (用户编写, 不会被 EEZ Studio 覆盖)
 *
 * 管理 dough_weight / sec (计时器) / motor_running 变量的 getter/setter
 * vars_init() 创建 LVGL 定时器每秒刷新 sec 和 motor_running
 */
#include "vars.h"
#include "motor.h"
#include "ui.h"
#include "esp_log.h"
#include "lvgl.h"

using namespace eez;

int32_t dough_weight = 50; // 默认50克

extern "C" {

int32_t get_var_dough_weight() {
    return dough_weight;
}

void set_var_dough_weight(int32_t value) {
    dough_weight = value;
    ESP_LOGI("VARS", "dough_weight updated to %ld g", (long)dough_weight);
}

/* ---- 计时器变量 (分:秒 格式) ---- */

int32_t timer_min;
int32_t timer_sec;

int32_t get_var_timer_min() {
    return timer_min;
}

void set_var_timer_min(int32_t value) {
    timer_min = value;
}

int32_t get_var_timer_sec() {
    return timer_sec;
}

void set_var_timer_sec(int32_t value) {
    timer_sec = value;
}

bool motor_running;

bool get_var_motor_running() {
    return motor_running;
}

void set_var_motor_running(bool value) {
    motor_running = value;
}

/* ---- LVGL 定时器: 每秒刷新计时 (MM:SS) 和运行状态 ---- */

static void vars_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    uint32_t elapsed = motor_get_elapsed_seconds();
    set_var_timer_min((int32_t)(elapsed / 60));
    set_var_timer_sec((int32_t)(elapsed % 60));
    if (elapsed == 0) {
        set_var_motor_running(false);
    }
}

void vars_init() {
    dough_weight = 200;
    timer_min = 0;
    timer_sec = 0;
    motor_running = false;

    lv_timer_create(vars_timer_cb, 1000, NULL);

    ESP_LOGI("VARS", "Variables initialized, dough_weight=%ld g", (long)dough_weight);
}

} /* extern "C" */
