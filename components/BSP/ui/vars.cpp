/**
 * @file    vars.cpp
 * @brief   EEZ UI 全局变量实现 (用户编写, 不会被 EEZ Studio 覆盖)
 *
 * 管理 dough_weight 变量的 getter/setter 和初始化
 */
#include "vars.h"
#include "motor.h"
#include "ui.h"
#include "esp_log.h"

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

void vars_init() {
    dough_weight = 200;  // 默认200g
    ESP_LOGI("VARS", "Variables initialized, dough_weight=%ld g", (long)dough_weight);
}

} /* extern "C" */
