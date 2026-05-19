#include "touch.h"
#include "iic.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch_ft6x36.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "TOUCH";

static esp_lcd_touch_handle_t touch_handle = NULL;

void touch_init(i2c_master_bus_handle_t i2c_bus)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << CTP_RST) | (1ULL << CTP_INT),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    gpio_set_level(CTP_RST, 1);

    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_FT6x36_CONFIG();
    io_config.scl_speed_hz = 400000;

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &io_config, &tp_io_handle));

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = CTP_RST,
        .int_gpio_num = CTP_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };

    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_ft6x36(tp_io_handle, &tp_cfg, &touch_handle));

    ESP_LOGI(TAG, "FT6336U touch controller initialized");
}

esp_lcd_touch_handle_t touch_get_handle(void)
{
    return touch_handle;
}
