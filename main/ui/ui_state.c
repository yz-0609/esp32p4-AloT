#include "ui_state.h"

#include <stdio.h>
#include <string.h>

#include "app_relay.h"
#include "app_weather.h"
#include "app_wifi.h"
#include "esp_log.h"

static const char *TAG = "ui_state";

static int clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static int calc_comfort(float temperature, int humidity, int light_lux, int air_quality)
{
    int score = 100;

    if (temperature < 20.0f || temperature > 30.0f) {
        score -= 18;
    } else if (temperature < 23.0f || temperature > 28.0f) {
        score -= 8;
    }

    if (humidity < 35 || humidity > 75) {
        score -= 16;
    } else if (humidity < 45 || humidity > 65) {
        score -= 6;
    }

    if (light_lux < 180) {
        score -= 18;
    } else if (light_lux < 300) {
        score -= 8;
    }

    if (air_quality < 60) {
        score -= 20;
    } else if (air_quality < 75) {
        score -= 8;
    }

    return clamp_int(score, 0, 100);
}

static bool sync_relay_outputs(ui_state_t *state)
{
    if (state == NULL) {
        return false;
    }

    esp_err_t ret = app_relay_set_all(state->relay_light_on, state->relay_fan_on);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "relay sync failed: %s", esp_err_to_name(ret));
        snprintf(state->last_feedback, sizeof(state->last_feedback), "Relay hardware update failed.");
        return false;
    }

    return true;
}

void ui_state_init(ui_state_t *state)
{
    if (state == NULL) {
        return;
    }

    memset(state, 0, sizeof(*state));
    state->relay_light_on = true;
    state->relay_fan_on = false;
    state->scene = UI_SCENE_STUDY;
    state->source = UI_SOURCE_BOOT;
    state->temperature = 26.4f;
    state->humidity = 58;
    state->light_lux = 320;
    state->air_quality = 82;
    state->comfort_score = calc_comfort(state->temperature, state->humidity, state->light_lux, state->air_quality);
    state->sensor_online = false;
    state->ai_connected = true;
    state->voice_ready = true;
    state->screen_brightness = 80;
    state->volume = 60;
    state->focus_minutes = 24;
    snprintf(state->time_text, sizeof(state->time_text), "08:30:00");
    snprintf(state->date_text, sizeof(state->date_text), "2026/06/13");
    snprintf(state->ai_user_text, sizeof(state->ai_user_text), "Adjust room for study.");
    snprintf(state->ai_reply_text, sizeof(state->ai_reply_text),
             "Light is a little low. Keep the lamp on and fan in auto mode.");
    snprintf(state->last_feedback, sizeof(state->last_feedback), "UI ready. Local control is available.");
    sync_relay_outputs(state);
}

void ui_state_poll_external(ui_state_t *state)
{
    if (state == NULL) {
        return;
    }

    state->wifi_connected = app_wifi_is_connected();
    state->time_synced = app_wifi_time_is_synced();

    if (state->time_synced) {
        char time_buffer[16] = {0};
        char date_buffer[32] = {0};

        if (app_wifi_format_time(time_buffer, sizeof(time_buffer), "%H:%M:%S") == ESP_OK) {
            snprintf(state->time_text, sizeof(state->time_text), "%s", time_buffer);
        }
        if (app_wifi_format_time(date_buffer, sizeof(date_buffer), "%Y/%m/%d") == ESP_OK) {
            snprintf(state->date_text, sizeof(state->date_text), "%s", date_buffer);
        }
    }

    app_weather_info_t weather = {0};
    if (app_weather_get_latest(&weather) == ESP_OK && weather.valid) {
        state->temperature = weather.temperature;
        state->humidity = weather.humidity;
        state->light_lux = weather.cloud_cover > 70 ? 180 : 360;
        state->air_quality = weather.wind_speed > 2.0f ? 86 : 74;
        state->sensor_online = true;
    }

    state->comfort_score = calc_comfort(state->temperature, state->humidity, state->light_lux, state->air_quality);
}

void ui_state_toggle_light(ui_state_t *state, ui_control_source_t source)
{
    if (state == NULL) {
        return;
    }
    state->relay_light_on = !state->relay_light_on;
    state->source = source;
    state->scene = UI_SCENE_MANUAL;
    if (sync_relay_outputs(state)) {
        snprintf(state->last_feedback, sizeof(state->last_feedback), "Light relay switched %s by %s.",
                 ui_state_onoff(state->relay_light_on), ui_state_source_name(source));
    }
}

void ui_state_toggle_fan(ui_state_t *state, ui_control_source_t source)
{
    if (state == NULL) {
        return;
    }
    state->relay_fan_on = !state->relay_fan_on;
    state->source = source;
    state->scene = UI_SCENE_MANUAL;
    if (sync_relay_outputs(state)) {
        snprintf(state->last_feedback, sizeof(state->last_feedback), "Vent relay switched %s by %s.",
                 ui_state_onoff(state->relay_fan_on), ui_state_source_name(source));
    }
}

void ui_state_apply_scene(ui_state_t *state, ui_scene_t scene, ui_control_source_t source)
{
    if (state == NULL) {
        return;
    }

    state->scene = scene;
    state->source = source;

    switch (scene) {
    case UI_SCENE_STUDY:
        state->relay_light_on = true;
        state->relay_fan_on = state->temperature > 28.0f || state->air_quality < 70;
        state->focus_minutes = 25;
        break;
    case UI_SCENE_REST:
        state->relay_light_on = false;
        state->relay_fan_on = false;
        state->focus_minutes = 0;
        break;
    case UI_SCENE_AWAY:
        state->relay_light_on = false;
        state->relay_fan_on = false;
        state->focus_minutes = 0;
        break;
    case UI_SCENE_AUTO:
        state->relay_light_on = state->light_lux < 260;
        state->relay_fan_on = state->temperature > 28.0f || state->air_quality < 76;
        break;
    case UI_SCENE_TIMER:
        state->focus_minutes = 30;
        break;
    case UI_SCENE_MANUAL:
    default:
        break;
    }

    if (sync_relay_outputs(state)) {
        snprintf(state->last_feedback, sizeof(state->last_feedback), "%s scene applied by %s.",
                 ui_state_scene_name(scene), ui_state_source_name(source));
    }
}

void ui_state_apply_ai_suggestion(ui_state_t *state)
{
    if (state == NULL) {
        return;
    }

    state->relay_light_on = true;
    state->relay_fan_on = state->temperature > 28.0f || state->air_quality < 75;
    state->scene = UI_SCENE_STUDY;
    state->source = UI_SOURCE_AI;
    snprintf(state->ai_user_text, sizeof(state->ai_user_text), "Make the room suitable for study.");
    snprintf(state->ai_reply_text, sizeof(state->ai_reply_text),
             "Applied study mode. Light is on and ventilation follows local comfort rules.");
    if (sync_relay_outputs(state)) {
        snprintf(state->last_feedback, sizeof(state->last_feedback), "AI command passed local safety checks.");
    }
}

void ui_state_adjust_volume(ui_state_t *state, int delta)
{
    if (state == NULL) {
        return;
    }
    state->volume = clamp_int(state->volume + delta, 0, 100);
    snprintf(state->last_feedback, sizeof(state->last_feedback), "Volume set to %d%%.", state->volume);
}

void ui_state_adjust_brightness(ui_state_t *state, int delta)
{
    if (state == NULL) {
        return;
    }
    state->screen_brightness = clamp_int(state->screen_brightness + delta, 0, 100);
    snprintf(state->last_feedback, sizeof(state->last_feedback), "Brightness set to %d%%.", state->screen_brightness);
}

const char *ui_state_scene_name(ui_scene_t scene)
{
    switch (scene) {
    case UI_SCENE_STUDY:
        return "Study";
    case UI_SCENE_REST:
        return "Rest";
    case UI_SCENE_AWAY:
        return "Away";
    case UI_SCENE_AUTO:
        return "Auto";
    case UI_SCENE_TIMER:
        return "Timer";
    case UI_SCENE_MANUAL:
    default:
        return "Manual";
    }
}

const char *ui_state_source_name(ui_control_source_t source)
{
    switch (source) {
    case UI_SOURCE_GUI:
        return "GUI";
    case UI_SOURCE_VOICE:
        return "Voice";
    case UI_SOURCE_RULE:
        return "Rule";
    case UI_SOURCE_AI:
        return "AI";
    case UI_SOURCE_BOOT:
    default:
        return "Boot";
    }
}

const char *ui_state_onoff(bool enabled)
{
    return enabled ? "ON" : "OFF";
}
