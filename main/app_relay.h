#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_RELAY_LIGHT = 0,
    APP_RELAY_FAN,
    APP_RELAY_COUNT,
} app_relay_channel_t;

esp_err_t app_relay_init(void);
esp_err_t app_relay_set(app_relay_channel_t channel, bool on);
esp_err_t app_relay_set_all(bool light_on, bool fan_on);
bool app_relay_get(app_relay_channel_t channel);

#ifdef __cplusplus
}
#endif
