#include "lcd.h"
#include "spi.h"
#include "esp_lcd_st7789.h"
#include "esp_log.h"

static const char *TAG = "LCD";

#define LCD_PIXEL_CLK_HZ    (40 * 1000 * 1000)
#define LCD_BITS_PER_PIXEL  16

void lcd_init(esp_lcd_panel_io_handle_t *ret_io, esp_lcd_panel_handle_t *ret_panel)
{
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_DC,
        .cs_gpio_num = LCD_CS,
        .pclk_hz = LCD_PIXEL_CLK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)spi_lcd_get_host(),
                                              &io_config, ret_io));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_RST,
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
        .rgb_endian = LCD_RGB_ENDIAN_RGB,
#else
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
#endif
        .bits_per_pixel = LCD_BITS_PER_PIXEL,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(*ret_io, &panel_config, ret_panel));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(*ret_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(*ret_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(*ret_panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(*ret_panel, 0, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(*ret_panel, true, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(*ret_panel, true));

    ESP_LOGI(TAG, "ST7789V LCD panel initialized");
}
