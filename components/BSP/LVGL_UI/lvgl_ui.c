#include "lvgl_ui.h"
#include "lcd.h"
#include "touch.h"
#include "esp_log.h"

static const char *TAG = "LVGL_UI";

static lv_disp_t *disp = NULL;

void lvgl_ui_init(i2c_master_bus_handle_t i2c_bus)
{
    lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    esp_lcd_panel_io_handle_t lcd_io = NULL;
    esp_lcd_panel_handle_t lcd_panel = NULL;
    lcd_init(&lcd_io, &lcd_panel);

    lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = lcd_io,
        .panel_handle = lcd_panel,
        .buffer_size = LCD_H_RES * 50,
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .rotation = {
            .swap_xy = false,
            .mirror_x = true,
            .mirror_y = true,
        },
        .flags = {
            .buff_dma = true,
        },
    };
    disp = lvgl_port_add_disp(&disp_cfg);

    touch_init(i2c_bus);

    lvgl_port_touch_cfg_t touch_cfg = {
        .disp = disp,
        .handle = touch_get_handle(),
    };
    lvgl_port_add_touch(&touch_cfg);

    ESP_LOGI(TAG, "LVGL UI initialized");
}

void lvgl_test_screen(void)
{
    lvgl_port_lock(0);

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);

    /* 标题 — Montserrat 20 */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Dough Mixer");
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    /* 中文标签 — Simsun 16 CJK */
    lv_obj_t *cjk = lv_label_create(scr);
    lv_label_set_text(cjk, "和面机控制面板");
    lv_obj_set_style_text_color(cjk, lv_color_hex(0xe94560), 0);
    lv_obj_set_style_text_font(cjk, &lv_font_simsun_16_cjk, 0);
    lv_obj_align(cjk, LV_ALIGN_TOP_MID, 0, 70);

    /* 状态 — Montserrat 14 */
    lv_obj_t *status = lv_label_create(scr);
    lv_label_set_text(status, "System Ready");
    lv_obj_set_style_text_color(status, lv_color_hex(0x16c79a), 0);
    lv_obj_set_style_text_font(status, &lv_font_montserrat_14, 0);
    lv_obj_align(status, LV_ALIGN_CENTER, 0, 0);

    /* 圆弧 — 视觉校验 */
    lv_obj_t *arc = lv_arc_create(scr);
    lv_obj_set_size(arc, 100, 100);
    lv_arc_set_value(arc, 75);
    lv_obj_align(arc, LV_ALIGN_BOTTOM_MID, 0, -50);

    /* 圆弧标签 */
    lv_obj_t *arc_label = lv_label_create(scr);
    lv_label_set_text(arc_label, "75%");
    lv_obj_set_style_text_font(arc_label, &lv_font_montserrat_14, 0);
    lv_obj_align(arc_label, LV_ALIGN_BOTTOM_MID, 0, -90);

    lvgl_port_unlock();
}
