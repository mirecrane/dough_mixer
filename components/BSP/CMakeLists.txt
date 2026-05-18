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
            ST7789V_LED)

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
            ST7789V_LED)


set(requires
            driver
            esp_wifi
            esp_event
            nvs_flash
            lwip
            esp_http_server)

idf_component_register(SRC_DIRS ${src_dirs}
                       INCLUDE_DIRS ${include_dirs}
                       REQUIRES ${requires}
                       EMBED_FILES HTTP/http.html)

component_compile_options(-ffast-math -O3 -Wno-error=format -Wno-format)