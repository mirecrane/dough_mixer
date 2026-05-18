#include "http.h"
#include "motor.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "HTTP";

/* 由 CMake EMBED_FILES 生成的符号，指向 http.html 的二进制数据 */
extern const unsigned char _binary_http_html_start[] asm("_binary_http_html_start");
extern const unsigned char _binary_http_html_end[]   asm("_binary_http_html_end");

/* 全局电机状态: "idle" | "kneading" | "fermenting" | "done" | "error" */
static const char *g_motor_state = "idle";
static TaskHandle_t g_motor_task_handle = NULL;

/* ---- FreeRTOS 任务：在独立上下文中执行 motor_coordinated_control ---- */
static void motor_coordinated_task(void *pvParameters)
{
    motor_coordinated_control();
    g_motor_state = "fermenting";
    g_motor_task_handle = NULL;
    ESP_LOGI(TAG, "motor_coordinated_control() completed, entering fermentation phase");
    vTaskDelete(NULL);
}

/* ---- GET / — 返回控制面板网页 ---- */
static esp_err_t root_get_handler(httpd_req_t *req)
{
    size_t html_size = _binary_http_html_end - _binary_http_html_start;
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_send(req, (const char *)_binary_http_html_start, html_size);
    return ESP_OK;
}

/* ---- POST /api/start — 启动电机协同控制 ---- */
static esp_err_t api_start_post_handler(httpd_req_t *req)
{
    char resp[64];

    if (strcmp(g_motor_state, "idle") == 0 || strcmp(g_motor_state, "done") == 0) {
        g_motor_state = "kneading";
        BaseType_t ret = xTaskCreate(motor_coordinated_task,
                                      "motor_coord", 8192, NULL, 5,
                                      &g_motor_task_handle);
        if (ret != pdPASS) {
            g_motor_state = "error";
            ESP_LOGE(TAG, "Failed to create motor task");
            snprintf(resp, sizeof(resp),
                     "{\"status\":\"error\",\"state\":\"error\"}");
        } else {
            ESP_LOGI(TAG, "motor_coordinated_control() started via API");
            snprintf(resp, sizeof(resp),
                     "{\"status\":\"ok\",\"state\":\"kneading\"}");
        }
    } else {
        snprintf(resp, sizeof(resp),
                 "{\"status\":\"busy\",\"state\":\"%s\"}", g_motor_state);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

/* ---- GET /api/status — 返回当前电机状态 ---- */
static esp_err_t api_status_get_handler(httpd_req_t *req)
{
    char resp[64];
    snprintf(resp, sizeof(resp), "{\"state\":\"%s\"}", g_motor_state);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

/* ---- POST /api/stop — 紧急停止 ---- */
static esp_err_t api_stop_post_handler(httpd_req_t *req)
{
    /* 先删除正在运行的电机任务，再执行硬件停止 */
    if (g_motor_task_handle != NULL) {
        vTaskDelete(g_motor_task_handle);
        g_motor_task_handle = NULL;
    }
    motor_emergency_stop();
    g_motor_state = "idle";
    ESP_LOGI(TAG, "Emergency stop executed via HTTP API");

    const char *resp = "{\"status\":\"ok\",\"state\":\"idle\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

/* ---- 注册所有 URI 处理器 ---- */
static void register_handlers(httpd_handle_t server)
{
    httpd_uri_t root = {
        .uri      = "/",
        .method   = HTTP_GET,
        .handler  = root_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &root);

    httpd_uri_t api_start = {
        .uri      = "/api/start",
        .method   = HTTP_POST,
        .handler  = api_start_post_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &api_start);

    httpd_uri_t api_status = {
        .uri      = "/api/status",
        .method   = HTTP_GET,
        .handler  = api_status_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &api_status);

    httpd_uri_t api_stop = {
        .uri      = "/api/stop",
        .method   = HTTP_POST,
        .handler  = api_stop_post_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &api_stop);
}

/* ---- 启动 HTTP 服务器 ---- */
void http_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 8;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        register_handlers(server);
        ESP_LOGI(TAG, "HTTP server started on port %d", config.server_port);
    } else {
        ESP_LOGE(TAG, "Failed to start HTTP server on port %d", config.server_port);
    }
}
