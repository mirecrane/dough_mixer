#include "iic.h"
#include "esp_log.h"

static const char *TAG = "IIC";

void iic_touch_init(i2c_master_bus_handle_t *ret_bus)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = CTP_SDA,
        .scl_io_num = CTP_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, ret_bus));

    ESP_LOGI(TAG, "I2C0 bus initialized for FT6336U touch");
}
