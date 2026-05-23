set(src_dirs
            GPTIM
            MOTOR
            UART
            WIFI
            SPI
            HTTP
            IIC
            FT6336U_TOUCH
            LVGL_UI
            ST7789V_LCD
            ui)

set(include_dirs
            GPTIM
            MOTOR
            UART
            WIFI
            SPI
            HTTP
            IIC
            FT6336U_TOUCH
            LVGL_UI
            ST7789V_LCD
            ui)


set(requires
            driver
            esp_wifi
            esp_event
            nvs_flash
            lwip
            esp_http_server
            esp_lcd
            esp_lcd_touch_ft5x06
            lvgl
            esp_lvgl_port)

idf_component_register(SRC_DIRS ${src_dirs}
                       INCLUDE_DIRS ${include_dirs}
                       REQUIRES ${requires}
                       EMBED_FILES HTTP/http.html)

# ui/ 下的 .c 文件引用了 C++ 头文件 eez-flow.h，强制用 C++ 编译
set_source_files_properties(
    ui/ui.c
    ui/screens.c
    ui/images.c
    ui/styles.c
    PROPERTIES LANGUAGE CXX
)

component_compile_options(
    -ffast-math -O3
    -Wno-error=format -Wno-format
    -Wno-deprecated-enum-enum-conversion
    -Wno-missing-field-initializers
    -fpermissive
)