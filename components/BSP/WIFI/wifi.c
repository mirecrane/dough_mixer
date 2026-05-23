#include "wifi.h"
#include "motor.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"

static const char *TAG = "WIFI";

// 为了保持逻辑一致并遵循“不修改其他代码”的原则，我们在这里实现一套 TCP 指令处理逻辑
// 如果未来需要优化，建议将指令解析逻辑提取到独立的公共组件中

static TaskHandle_t wifi_timed_task_handle = NULL;

static void wifi_send_msg(int sock, const char *msg)
{
    if (sock >= 0) {
        send(sock, msg, strlen(msg), 0);
        send(sock, "\r\n", 2, 0);
    }
}

typedef struct {
    char type; // 'P', 'D', 'M', 'G'
    int val;   // seconds or minutes
    int sock;
} wifi_timed_cmd_t;

static void wifi_timed_action_task(void *pvParameters)
{
    wifi_timed_cmd_t *cmd = (wifi_timed_cmd_t *)pvParameters;
    char msg[64];
    
    if (cmd->type == 'P') {
        snprintf(msg, sizeof(msg), "Pump starting for %d seconds...", cmd->val);
        wifi_send_msg(cmd->sock, msg);
        pump_start();
        vTaskDelay(pdMS_TO_TICKS(cmd->val * 1000));
        pump_stop();
        wifi_send_msg(cmd->sock, "Pump stopped.");
    } else if (cmd->type == 'D') {
        snprintf(msg, sizeof(msg), "Dough motor starting for %d seconds...", cmd->val);
        wifi_send_msg(cmd->sock, msg);
        dough_esc_init_and_start();
        vTaskDelay(pdMS_TO_TICKS(cmd->val * 1000));
        dough_esc_stop();
        wifi_send_msg(cmd->sock, "Dough motor stopped.");
    } else if (cmd->type == 'M') { // Mixer
        snprintf(msg, sizeof(msg), "Mixer motor starting for %d minutes...", cmd->val);
        wifi_send_msg(cmd->sock, msg);
        mixer_esc_init_and_start();
        vTaskDelay(pdMS_TO_TICKS(cmd->val * 60 * 1000));
        mixer_esc_stop();
        wifi_send_msg(cmd->sock, "Mixer motor stopped.");
    } else if (cmd->type == 'G') {
        snprintf(msg, sizeof(msg), "Grinder starting for %d seconds...", cmd->val);
        wifi_send_msg(cmd->sock, msg);
        grinder_start();
        vTaskDelay(pdMS_TO_TICKS(cmd->val * 1000));
        grinder_stop();
        wifi_send_msg(cmd->sock, "Grinder stopped.");
    }
    
    free(cmd);
    wifi_timed_task_handle = NULL;
    vTaskDelete(NULL);
}

static void wifi_start_timed_task(int sock, char type, int val)
{
    if (wifi_timed_task_handle != NULL) {
        vTaskDelete(wifi_timed_task_handle);
        motor_emergency_stop();
    }
    
    wifi_timed_cmd_t *cmd = malloc(sizeof(wifi_timed_cmd_t));
    cmd->type = type;
    cmd->val = val;
    cmd->sock = sock;
    xTaskCreate(wifi_timed_action_task, "wifi_timed_act", 4096, cmd, 5, &wifi_timed_task_handle);
}

static void wifi_process_cmd(int sock, char *str)
{
    while(*str == ' ') str++;
    int len = strlen(str);
    while(len > 0 && (str[len-1] == ' ' || str[len-1] == '\r' || str[len-1] == '\n')) {
        str[--len] = '\0';
    }
    if (len == 0) return;

    ESP_LOGI(TAG, "TCP Received command: [%s]", str);

    char msg[64];

    if (strcasecmp(str, "STOP") == 0) {
        if (wifi_timed_task_handle != NULL) {
            vTaskDelete(wifi_timed_task_handle);
            wifi_timed_task_handle = NULL;
        }
        motor_emergency_stop();
        wifi_send_msg(sock, "Emergency Stop All: SUCCESS");
        return;
    }

    if (strncasecmp(str, "SS A ", 5) == 0) {
        float angle = atof(str + 5);
        steering_servo_set_angle(angle);
        snprintf(msg, sizeof(msg), "Steering servo set to %.1f deg: SUCCESS", angle);
        wifi_send_msg(sock, msg);
        return;
    }

    if (strncasecmp(str, "SP A ", 5) == 0) {
        float angle = atof(str + 5);
        push_servo_set_angle(angle);
        snprintf(msg, sizeof(msg), "Push servo set to %.1f deg: SUCCESS", angle);
        wifi_send_msg(sock, msg);
        return;
    }

    if (strncasecmp(str, "M ", 2) == 0) {
        char *sub = str + 2;
        if (strncasecmp(sub, "A ", 2) == 0) {
            float angle = atof(sub + 2);
            step_rotate_angle(angle, DIR_CW, 1000);
            snprintf(msg, sizeof(msg), "Stepper rotated %.1f deg: SUCCESS", angle);
            wifi_send_msg(sock, msg);
        } else if (strcasecmp(sub, "START") == 0) {
            step_start_auto_mode();
            wifi_send_msg(sock, "Stepper auto mode started: SUCCESS");
        } else if (strncasecmp(sub, "B ", 2) == 0) {
            float angle = atof(sub + 2);
            step_rotate_angle(angle, DIR_CCW, 1000);
            snprintf(msg, sizeof(msg), "Stepper rotated -%.1f deg: SUCCESS", angle);
            wifi_send_msg(sock, msg);
        } else if (strcasecmp(sub, "STOP") == 0) {
            step_stop_auto_mode();
            wifi_send_msg(sock, "Stepper auto mode stopped: SUCCESS");
        } else if (strncasecmp(sub, "F ", 2) == 0) {
            float turns = atof(sub + 2);
            step_rotate_turns(turns, DIR_CW, 1000);
            snprintf(msg, sizeof(msg), "Stepper rotated %.1f turns: SUCCESS", turns);
            wifi_send_msg(sock, msg);
        } else if (strncasecmp(sub, "R ", 2) == 0) {
            float turns = atof(sub + 2);
            step_rotate_turns(turns, DIR_CCW, 1000);
            snprintf(msg, sizeof(msg), "Stepper rotated -%.1f turns: SUCCESS", turns);
            wifi_send_msg(sock, msg);
        }
        return;
    }

    if (strncasecmp(str, "P ", 2) == 0) {
        int sec = atoi(str + 2);
        wifi_start_timed_task(sock, 'P', sec);
        return;
    }

    if (strncasecmp(str, "D ", 2) == 0) {
        int sec = atoi(str + 2);
        wifi_start_timed_task(sock, 'D', sec);
        return;
    }

    if (strncasecmp(str, "MI ", 3) == 0) {
        int min = atoi(str + 3);
        wifi_start_timed_task(sock, 'M', min);
        return;
    }

    if (strncasecmp(str, "G ", 2) == 0) {
        int sec = atoi(str + 2);
        wifi_start_timed_task(sock, 'G', sec);
        return;
    }

    if (strcasecmp(str, "H") == 0) {
        motor_coordinated_control();
        wifi_send_msg(sock, "Coordinated control started: SUCCESS");
        return;
    }

    wifi_send_msg(sock, "Error: Unknown TCP command");
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

void wifi_init_softap(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .channel = WIFI_CHANNEL,
            .password = WIFI_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        },
    };
    if (strlen(WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             WIFI_SSID, WIFI_PASS, WIFI_CHANNEL);
}

void tcp_server_task(void *pvParameters)
{
    char rx_buffer[128];
    char addr_str[32];
    int addr_family = (int)pvParameters;
    int ip_protocol = 0;
    struct sockaddr_storage dest_addr;

    if (addr_family == AF_INET) {
        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
        dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(TCP_PORT);
        ip_protocol = IPPROTO_IP;
    }

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    ESP_LOGI(TAG, "Socket created");

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        goto CLEAN_UP;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", TCP_PORT);

    err = listen(listen_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }

    while (1) {
        ESP_LOGI(TAG, "Socket listening");

        struct sockaddr_storage source_addr;
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        if (source_addr.ss_family == AF_INET) {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        }
        ESP_LOGI(TAG, "Socket accepted ip: %s", addr_str);

        while (1) {
            int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            if (len < 0) {
                ESP_LOGE(TAG, "recv failed: errno %d", errno);
                break;
            } else if (len == 0) {
                ESP_LOGI(TAG, "Connection closed");
                break;
            } else {
                rx_buffer[len] = 0;
                wifi_process_cmd(sock, rx_buffer);
            }
        }

        if (sock != -1) {
            shutdown(sock, 0);
            close(sock);
        }
    }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}
