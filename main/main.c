#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "motor.h"
#include "uart.h"
#include "wifi.h"
#include "http.h"
#include "esp_log.h"

void app_main(void)
{
    // 初始化电机 (包括步进电机、PWM 电机和继电器)
    step_init();
    
    // 初始化串口
    uart_init();
    
    // 初始化 WiFi AP
    wifi_init_softap();
    
    // 启动串口监听任务 (支持 UART0 和 UART1)
    xTaskCreate(uart_command_task, "uart1_cmd", 4096, NULL, 10, NULL);
    xTaskCreate(uart0_command_task, "uart0_cmd", 4096, NULL, 10, NULL);
    
    // 启动 TCP 服务器任务 (监听 8080 端口)
    xTaskCreate(tcp_server_task, "tcp_server", 4096, (void*)AF_INET, 5, NULL);

    // 启动 HTTP 服务器 (监听 80 端口，提供网页控制面板)
    http_server_start();
    
    ESP_LOGI("MAIN", "System Started, WiFi SSID: dough_mixer, PWD: 12345678");
    ESP_LOGI("MAIN", "TCP Server listening on port 8080");
    ESP_LOGI("MAIN", "HTTP Server listening on port 80");

    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
