#ifndef __WIFI_H__
#define __WIFI_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "lwip/err.h"
#include "lwip/sockets.h"

#define WIFI_SSID      "dough_mixer"
#define WIFI_PASS      "12345678"
#define WIFI_CHANNEL   1
#define MAX_STA_CONN   4

#define TCP_PORT       8080

/**
 * @brief 初始化 WiFi AP 模式
 */
void wifi_init_softap(void);

/**
 * @brief TCP 服务器任务
 */
void tcp_server_task(void *pvParameters);

#endif
