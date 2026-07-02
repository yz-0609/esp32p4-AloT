#include "app_relay.h"

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"

#define APP_RELAY1_GPIO GPIO_NUM_32
#define APP_RELAY2_GPIO GPIO_NUM_46
#define APP_RELAY_ACTIVE_LEVEL 1

static const char *TAG = "app_relay";

static const gpio_num_t s_relay_gpios[APP_RELAY_COUNT] = {
    [APP_RELAY_LIGHT] = APP_RELAY1_GPIO,
    [APP_RELAY_FAN] = APP_RELAY2_GPIO,
};

static bool s_relay_states[APP_RELAY_COUNT];
static bool s_relay_initialized;

static int relay_level(bool on)
{
    return on ? APP_RELAY_ACTIVE_LEVEL : !APP_RELAY_ACTIVE_LEVEL;
}

static esp_err_t check_channel(app_relay_channel_t channel)
{
    ESP_RETURN_ON_FALSE(channel >= 0 && channel < APP_RELAY_COUNT, ESP_ERR_INVALID_ARG, TAG,
                        "invalid relay channel: %d", (int)channel);
    return ESP_OK;
}

esp_err_t app_relay_init(void)
{
    if (s_relay_initialized) {
        return ESP_OK;
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << APP_RELAY1_GPIO) | (1ULL << APP_RELAY2_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_RETURN_ON_ERROR(gpio_config(&io_conf), TAG, "configure relay GPIOs failed");

    for (int i = 0; i < APP_RELAY_COUNT; ++i) {
        ESP_RETURN_ON_ERROR(gpio_set_level(s_relay_gpios[i], relay_level(false)), TAG,
                            "set relay %d safe level failed", i);
        s_relay_states[i] = false;
    }

    s_relay_initialized = true;
    ESP_LOGI(TAG, "relay initialized: relay1=GPIO%d relay2=GPIO%d active_level=%d",
             APP_RELAY1_GPIO, APP_RELAY2_GPIO, APP_RELAY_ACTIVE_LEVEL);
    return ESP_OK;
}

esp_err_t app_relay_set(app_relay_channel_t channel, bool on)
{
    ESP_RETURN_ON_ERROR(check_channel(channel), TAG, "relay channel check failed");
    ESP_RETURN_ON_FALSE(s_relay_initialized, ESP_ERR_INVALID_STATE, TAG, "relay is not initialized");

    ESP_RETURN_ON_ERROR(gpio_set_level(s_relay_gpios[channel], relay_level(on)), TAG,
                        "set relay %d failed", (int)channel);
    s_relay_states[channel] = on;
    ESP_LOGI(TAG, "relay %d %s", (int)channel + 1, on ? "ON" : "OFF");
    return ESP_OK;
}

esp_err_t app_relay_set_all(bool light_on, bool fan_on)
{
    ESP_RETURN_ON_ERROR(app_relay_set(APP_RELAY_LIGHT, light_on), TAG, "set light relay failed");
    ESP_RETURN_ON_ERROR(app_relay_set(APP_RELAY_FAN, fan_on), TAG, "set fan relay failed");
    return ESP_OK;
}

bool app_relay_get(app_relay_channel_t channel)
{
    if (check_channel(channel) != ESP_OK) {
        return false;
    }
    return s_relay_states[channel];
}
