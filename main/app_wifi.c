#include "app_wifi.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"

#define APP_WIFI_SSID          "Lab107_AX6"
#define APP_WIFI_PASSWORD      "lab120120."
#define APP_WIFI_MAXIMUM_RETRY 10
#define APP_TIME_SYNC_RETRY_PERIOD_MS 3000

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "app_wifi";

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num;
static bool s_connected;
static bool s_sntp_initialized;
static bool s_time_synced;

static void app_wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

        if (s_retry_num < APP_WIFI_MAXIMUM_RETRY) {
            s_retry_num++;
            ESP_LOGW(TAG, "Wi-Fi disconnected, retrying (%d/%d)", s_retry_num, APP_WIFI_MAXIMUM_RETRY);
            esp_wifi_connect();
        } else {
            ESP_LOGE(TAG, "Wi-Fi connect failed after retries");
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        s_retry_num = 0;
        s_connected = true;
        ESP_LOGI(TAG, "Connected to %s, got IP: " IPSTR, APP_WIFI_SSID, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static esp_err_t app_time_sync(void)
{
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("ntp.aliyun.com");
    ESP_RETURN_ON_ERROR(esp_netif_sntp_init(&config), TAG, "SNTP init failed");
    s_sntp_initialized = true;

    esp_err_t ret = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(15000));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SNTP sync failed or timed out: %s", esp_err_to_name(ret));
        esp_netif_sntp_deinit();
        s_sntp_initialized = false;
        return ret;
    }

    setenv("TZ", "CST-8", 1);
    tzset();
    s_time_synced = true;

    ESP_LOGI(TAG, "SNTP time synchronized");
    return ESP_OK;
}

static esp_err_t app_wifi_init_sta(void)
{
    esp_err_t ret = ESP_OK;

    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }

    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    if (sta_netif == NULL) {
        ESP_LOGW(TAG, "Default Wi-Fi STA netif may already exist");
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "esp_wifi_init failed");

    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                            &app_wifi_event_handler, NULL, NULL),
                        TAG, "register WIFI_EVENT handler failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                            &app_wifi_event_handler, NULL, NULL),
                        TAG, "register IP_EVENT handler failed");

    wifi_config_t wifi_config = {0};
    strlcpy((char *)wifi_config.sta.ssid, APP_WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, APP_WIFI_PASSWORD, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "esp_wifi_set_mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG, "esp_wifi_set_config failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "esp_wifi_start failed");

    ESP_LOGI(TAG, "Connecting to Wi-Fi SSID: %s", APP_WIFI_SSID);

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(30000));

    if (bits & WIFI_CONNECTED_BIT) {
        return ESP_OK;
    }
    if (bits & WIFI_FAIL_BIT) {
        return ESP_FAIL;
    }

    ESP_LOGW(TAG, "Wi-Fi connection timed out");
    return ESP_ERR_TIMEOUT;
}

static void app_wifi_task(void *arg)
{
    esp_err_t ret = app_wifi_init_sta();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi initialization/connect failed: %s", esp_err_to_name(ret));
    } else {
        while (app_time_sync() != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(APP_TIME_SYNC_RETRY_PERIOD_MS));
        }

        while (true) {
            vTaskDelay(pdMS_TO_TICKS(60000));
        }
    }

    if (s_sntp_initialized) {
        esp_netif_sntp_deinit();
        s_sntp_initialized = false;
    }

    vTaskDelete(NULL);
}

esp_err_t app_wifi_start(void)
{
    BaseType_t ok = xTaskCreate(app_wifi_task, "app_wifi", 4096, NULL, 4, NULL);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

bool app_wifi_is_connected(void)
{
    return s_connected;
}

bool app_wifi_time_is_synced(void)
{
    return s_time_synced;
}

esp_err_t app_wifi_get_time(struct tm *timeinfo)
{
    if (timeinfo == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_time_synced) {
        return ESP_ERR_INVALID_STATE;
    }

    time_t now = 0;
    time(&now);
    localtime_r(&now, timeinfo);
    return ESP_OK;
}

esp_err_t app_wifi_format_time(char *buffer, size_t buffer_size, const char *format)
{
    if (buffer == NULL || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    struct tm timeinfo = {0};
    esp_err_t ret = app_wifi_get_time(&timeinfo);
    if (ret != ESP_OK) {
        buffer[0] = '\0';
        return ret;
    }

    const char *fmt = format != NULL ? format : "%Y-%m-%d %H:%M:%S";
    if (strftime(buffer, buffer_size, fmt, &timeinfo) == 0) {
        buffer[0] = '\0';
        return ESP_ERR_INVALID_SIZE;
    }

    return ESP_OK;
}
