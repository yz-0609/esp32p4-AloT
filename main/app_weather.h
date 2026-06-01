#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool valid;
    float temperature;
    float apparent_temperature;
    int humidity;
    int weather_code;
    float wind_speed;
    int wind_direction;
    float precipitation;
    int cloud_cover;
    char description[32];
    char update_time[32];
} app_weather_info_t;

esp_err_t app_weather_start(void);
bool app_weather_is_valid(void);
esp_err_t app_weather_get_latest(app_weather_info_t *info);

#ifdef __cplusplus
}
#endif
