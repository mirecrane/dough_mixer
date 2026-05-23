#include "actions.h"
#include "vars.h"
#include "motor.h"
#include "ui.h"
#include "esp_log.h"

using namespace eez;

static const char *TAG = "ACTIONS";

extern "C" {

void action_dough_sub(lv_event_t *e)
{
    int32_t w = get_var_dough_weight();
    if (w > 50) {
        set_var_dough_weight(w - 50);
    }
    ESP_LOGI(TAG, "Dough sub: %ld -> %ld g", (long)w, (long)get_var_dough_weight());
}

void action_dough_add(lv_event_t *e)
{
    int32_t w = get_var_dough_weight();
    set_var_dough_weight(w + 50);
    ESP_LOGI(TAG, "Dough add: %ld -> %ld g", (long)w, (long)get_var_dough_weight());
}

void action_set_200g(lv_event_t *e)
{
    set_var_dough_weight(200);
    ESP_LOGI(TAG, "Preset: 200g");
}

void action_set_300g(lv_event_t *e)
{
    set_var_dough_weight(300);
    ESP_LOGI(TAG, "Preset: 300g");
}

void action_set_500g(lv_event_t *e)
{
    set_var_dough_weight(500);
    ESP_LOGI(TAG, "Preset: 500g");
}

void action_set_1000g(lv_event_t *e)
{
    set_var_dough_weight(1000);
    ESP_LOGI(TAG, "Preset: 1000g");
}

void action_star_mixer(lv_event_t *e)
{
    int32_t w = get_var_dough_weight();
    ESP_LOGI(TAG, "Start mixer, weight=%ldg", (long)w);
    motor_coordinated_control_ui_http((uint32_t)w);
}

void action_stop(lv_event_t *e)
{
    ESP_LOGI(TAG, "Emergency stop");
    motor_emergency_stop();
}

} /* extern "C" */
