#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t app_wifi_start(void);
bool app_wifi_is_connected(void);
bool app_wifi_time_is_synced(void);
esp_err_t app_wifi_get_time(struct tm *timeinfo);
esp_err_t app_wifi_format_time(char *buffer, size_t buffer_size, const char *format);

#ifdef __cplusplus
}
#endif
