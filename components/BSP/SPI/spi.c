#include "spi.h"

static const char *TAG = "SPI";

static spi_host_device_t spi_host = SPI2_HOST;

void spi_lcd_init(void)
{
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = LCD_MOSI,
        .miso_io_num = LCD_MISO,
        .sclk_io_num = LCD_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 800 * 480 * 2,
    };

    ESP_ERROR_CHECK(spi_bus_initialize(spi_host, &bus_cfg, SPI_DMA_CH_AUTO));

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LCD_BL),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    gpio_set_level(LCD_BL, 1);

    ESP_LOGI(TAG, "SPI2 bus initialized for LCD");
}

spi_host_device_t spi_lcd_get_host(void)
{
    return spi_host;
}
