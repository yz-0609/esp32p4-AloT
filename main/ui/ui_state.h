#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_SCENE_MANUAL = 0,
    UI_SCENE_STUDY,
    UI_SCENE_REST,
    UI_SCENE_AWAY,
    UI_SCENE_AUTO,
    UI_SCENE_TIMER,
} ui_scene_t;

typedef enum {
    UI_SOURCE_BOOT = 0,
    UI_SOURCE_GUI,
    UI_SOURCE_VOICE,
    UI_SOURCE_RULE,
    UI_SOURCE_AI,
} ui_control_source_t;

typedef struct {
    bool relay_light_on;
    bool relay_fan_on;
    ui_scene_t scene;
    ui_control_source_t source;

    float temperature;
    int humidity;
    int light_lux;
    int air_quality;
    int comfort_score;
    bool sensor_online;

    bool wifi_connected;
    bool time_synced;
    bool ai_connected;
    bool voice_ready;

    int screen_brightness;
    int volume;
    int focus_minutes;

    char time_text[16];
    char date_text[32];
    char ai_user_text[128];
    char ai_reply_text[240];
    char last_feedback[160];
} ui_state_t;

void ui_state_init(ui_state_t *state);
void ui_state_poll_external(ui_state_t *state);
void ui_state_toggle_light(ui_state_t *state, ui_control_source_t source);
void ui_state_toggle_fan(ui_state_t *state, ui_control_source_t source);
void ui_state_apply_scene(ui_state_t *state, ui_scene_t scene, ui_control_source_t source);
void ui_state_apply_ai_suggestion(ui_state_t *state);
void ui_state_adjust_volume(ui_state_t *state, int delta);
void ui_state_adjust_brightness(ui_state_t *state, int delta);

const char *ui_state_scene_name(ui_scene_t scene);
const char *ui_state_source_name(ui_control_source_t source);
const char *ui_state_onoff(bool enabled);

#ifdef __cplusplus
}
#endif
