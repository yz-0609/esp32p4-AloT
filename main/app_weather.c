#include "app_weather.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "cJSON.h"
#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "app_wifi.h"

#define APP_WEATHER_URL "https://api.open-meteo.com/v1/forecast?latitude=31.2304&longitude=121.4737&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m,wind_direction_10m,precipitation,cloud_cover&timezone=Asia%2FShanghai"
#define APP_WEATHER_RESPONSE_MAX_BYTES 4096
#define APP_WEATHER_UPDATE_PERIOD_MS   (10 * 60 * 1000)
#define APP_WEATHER_RETRY_PERIOD_MS    30000
#define APP_WEATHER_WIFI_WAIT_MS       1000

static const char *TAG = "app_weather";

typedef struct {
    char *buffer;
    int length;
    int capacity;
} app_weather_http_buffer_t;

static SemaphoreHandle_t s_weather_mutex;
static TaskHandle_t s_weather_task_handle;
static app_weather_info_t s_weather;

static const char *app_weather_code_to_desc(int code)
{
    if (code == 0) {
        return "Clear";
    }
    if (code >= 1 && code <= 3) {
        return "Cloudy";
    }
    if (code == 45 || code == 48) {
        return "Fog";
    }
    if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) {
        return "Rain";
    }
    if (code >= 71 && code <= 77) {
        return "Snow";
    }
    if (code >= 95 && code <= 99) {
        return "Thunderstorm";
    }
    return "Unknown";
}

static cJSON *app_weather_get_required_item(cJSON *object, const char *name)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsNumber(item) ? item : NULL;
}

static esp_err_t app_weather_parse_current(const char *json, app_weather_info_t *weather)
{
    cJSON *root = cJSON_Parse(json);
    ESP_RETURN_ON_FALSE(root != NULL, ESP_FAIL, TAG, "parse weather JSON failed");

    esp_err_t ret = ESP_OK;
    cJSON *current = cJSON_GetObjectItemCaseSensitive(root, "current");
    ESP_GOTO_ON_FALSE(cJSON_IsObject(current), ESP_FAIL, cleanup, TAG, "missing current weather object");

    cJSON *temperature = app_weather_get_required_item(current, "temperature_2m");
    cJSON *apparent_temperature = app_weather_get_required_item(current, "apparent_temperature");
    cJSON *humidity = app_weather_get_required_item(current, "relative_humidity_2m");
    cJSON *weather_code = app_weather_get_required_item(current, "weather_code");
    cJSON *wind_speed = app_weather_get_required_item(current, "wind_speed_10m");
    cJSON *wind_direction = app_weather_get_required_item(current, "wind_direction_10m");
    cJSON *precipitation = app_weather_get_required_item(current, "precipitation");
    cJSON *cloud_cover = app_weather_get_required_item(current, "cloud_cover");
    cJSON *time = cJSON_GetObjectItemCaseSensitive(current, "time");

    ESP_GOTO_ON_FALSE(temperature && apparent_temperature && humidity && weather_code &&
                      wind_speed && wind_direction && precipitation && cloud_cover,
                      ESP_FAIL, cleanup, TAG, "weather JSON is missing required fields");

    memset(weather, 0, sizeof(*weather));
    weather->valid = true;
    weather->temperature = (float)temperature->valuedouble;
    weather->apparent_temperature = (float)apparent_temperature->valuedouble;
    weather->humidity = humidity->valueint;
    weather->weather_code = weather_code->valueint;
    weather->wind_speed = (float)wind_speed->valuedouble;
    weather->wind_direction = wind_direction->valueint;
    weather->precipitation = (float)precipitation->valuedouble;
    weather->cloud_cover = cloud_cover->valueint;
    strlcpy(weather->description, app_weather_code_to_desc(weather->weather_code), sizeof(weather->description));
    if (cJSON_IsString(time) && time->valuestring != NULL) {
        strlcpy(weather->update_time, time->valuestring, sizeof(weather->update_time));
    }

cleanup:
    cJSON_Delete(root);
    return ret;
}

static esp_err_t app_weather_http_event_handler(esp_http_client_event_t *evt)
{
    app_weather_http_buffer_t *response = (app_weather_http_buffer_t *)evt->user_data;

    if (evt->event_id == HTTP_EVENT_ON_DATA && response != NULL && evt->data_len > 0) {
        if (response->length + evt->data_len >= response->capacity) {
            ESP_LOGE(TAG, "weather response is too large");
            return ESP_FAIL;
        }

        memcpy(response->buffer + response->length, evt->data, evt->data_len);
        response->length += evt->data_len;
        response->buffer[response->length] = '\0';
    }

    return ESP_OK;
}

static esp_err_t app_weather_fetch(app_weather_info_t *weather)
{
    app_weather_http_buffer_t response = {
        .buffer = calloc(1, APP_WEATHER_RESPONSE_MAX_BYTES),
        .capacity = APP_WEATHER_RESPONSE_MAX_BYTES,
    };
    ESP_RETURN_ON_FALSE(response.buffer != NULL, ESP_ERR_NO_MEM, TAG, "allocate weather response buffer failed");

    esp_http_client_config_t config = {
        .url = APP_WEATHER_URL,
        .event_handler = app_weather_http_event_handler,
        .user_data = &response,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        free(response.buffer);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = esp_http_client_perform(client);
    if (ret == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        if (status == 200) {
            ret = app_weather_parse_current(response.buffer, weather);
        } else {
            ESP_LOGW(TAG, "weather request failed, HTTP status: %d", status);
            ret = ESP_FAIL;
        }
    } else {
        ESP_LOGW(TAG, "weather request failed: %s", esp_err_to_name(ret));
    }

    esp_http_client_cleanup(client);
    free(response.buffer);
    return ret;
}

static void app_weather_store_latest(const app_weather_info_t *weather)
{
    if (xSemaphoreTake(s_weather_mutex, portMAX_DELAY) == pdTRUE) {
        s_weather = *weather;
        xSemaphoreGive(s_weather_mutex);
    }
}

static void app_weather_task(void *arg)
{
    while (!app_wifi_is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(APP_WEATHER_WIFI_WAIT_MS));
    }

    while (true) {
        app_weather_info_t weather = {0};
        esp_err_t ret = app_weather_fetch(&weather);
        if (ret == ESP_OK) {
            app_weather_store_latest(&weather);
            ESP_LOGI(TAG, "Shanghai weather: %s, %.1f C, feels %.1f C, humidity %d%%, wind %.1f km/h, cloud %d%%, precipitation %.1f mm, time %s",
                     weather.description,
                     weather.temperature,
                     weather.apparent_temperature,
                     weather.humidity,
                     weather.wind_speed,
                     weather.cloud_cover,
                     weather.precipitation,
                     weather.update_time);
            vTaskDelay(pdMS_TO_TICKS(APP_WEATHER_UPDATE_PERIOD_MS));
        } else {
            vTaskDelay(pdMS_TO_TICKS(APP_WEATHER_RETRY_PERIOD_MS));
        }
    }
}

esp_err_t app_weather_start(void)
{
    if (s_weather_mutex == NULL) {
        s_weather_mutex = xSemaphoreCreateMutex();
        if (s_weather_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    if (s_weather_task_handle != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    BaseType_t ok = xTaskCreate(app_weather_task, "app_weather", 8192, NULL, 4, &s_weather_task_handle);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

bool app_weather_is_valid(void)
{
    bool valid = false;

    if (s_weather_mutex != NULL && xSemaphoreTake(s_weather_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        valid = s_weather.valid;
        xSemaphoreGive(s_weather_mutex);
    }

    return valid;
}

esp_err_t app_weather_get_latest(app_weather_info_t *info)
{
    if (info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_weather_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(s_weather_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (s_weather.valid) {
            *info = s_weather;
            ret = ESP_OK;
        }
        xSemaphoreGive(s_weather_mutex);
    } else {
        ret = ESP_ERR_TIMEOUT;
    }

    return ret;
}
