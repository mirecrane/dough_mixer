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
