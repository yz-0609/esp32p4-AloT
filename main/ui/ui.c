#include "ui.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "lvgl.h"
#include "ui_state.h"

#define UI_W 720
#define UI_H 720
#define UI_BG 0xF7F9FA
#define UI_INK 0x172026
#define UI_MUTED 0x6E7B83
#define UI_ACCENT 0x28A9A1

typedef enum {
    UI_PAGE_HOME = 0,
    UI_PAGE_CONTROL,
    UI_PAGE_ENV,
    UI_PAGE_SCENE,
    UI_PAGE_AI,
    UI_PAGE_SETTINGS,
    UI_PAGE_COUNT,
} ui_page_t;

typedef struct {
    ui_state_t state;
    ui_page_t current_page;
    bool waiting_for_time;
    lv_obj_t *screen;
    lv_timer_t *timer;

    lv_obj_t *time_label;
    lv_obj_t *date_label;
    lv_obj_t *standby_status_label;
    lv_obj_t *standby_detail_label;
    lv_obj_t *wifi_pill;
    lv_obj_t *light_value;
    lv_obj_t *fan_value;
    lv_obj_t *temp_value;
    lv_obj_t *humidity_value;
    lv_obj_t *lux_value;
    lv_obj_t *air_value;
    lv_obj_t *comfort_value;
    lv_obj_t *feedback_label;
} ui_runtime_t;

static ui_runtime_t s_ui;
static lv_style_t s_screen_style;
static lv_style_t s_card_style;
static lv_style_t s_card_accent_style;
static lv_style_t s_button_style;
static lv_style_t s_button_active_style;
static lv_style_t s_pill_style;
static lv_style_t s_nav_style;
static lv_style_t s_nav_active_style;
static bool s_styles_ready;

static void ui_open_page(ui_page_t page);

static const lv_font_t *font_cn_16(void)
{
    return &lv_font_montserrat_18;
}

static const lv_font_t *font_cn_24(void)
{
    return &lv_font_montserrat_24;
}

static void style_init_once(void)
{
    if (s_styles_ready) {
        return;
    }

    lv_style_init(&s_screen_style);
    lv_style_set_bg_color(&s_screen_style, lv_color_hex(UI_BG));
    lv_style_set_bg_opa(&s_screen_style, LV_OPA_COVER);
    lv_style_set_border_width(&s_screen_style, 0);
    lv_style_set_pad_all(&s_screen_style, 0);

    lv_style_init(&s_card_style);
    lv_style_set_bg_color(&s_card_style, lv_color_hex(UI_BG));
    lv_style_set_bg_opa(&s_card_style, LV_OPA_COVER);
    lv_style_set_border_width(&s_card_style, 2);
    lv_style_set_border_color(&s_card_style, lv_color_hex(UI_MUTED));
    lv_style_set_radius(&s_card_style, 8);
    lv_style_set_pad_all(&s_card_style, 0);

    lv_style_init(&s_card_accent_style);
    lv_style_set_bg_color(&s_card_accent_style, lv_color_hex(UI_BG));
    lv_style_set_bg_opa(&s_card_accent_style, LV_OPA_COVER);
    lv_style_set_border_width(&s_card_accent_style, 2);
    lv_style_set_border_color(&s_card_accent_style, lv_color_hex(UI_ACCENT));
    lv_style_set_radius(&s_card_accent_style, 8);
    lv_style_set_pad_all(&s_card_accent_style, 0);

    lv_style_init(&s_button_style);
    lv_style_set_bg_opa(&s_button_style, LV_OPA_TRANSP);
    lv_style_set_border_width(&s_button_style, 2);
    lv_style_set_border_color(&s_button_style, lv_color_hex(UI_ACCENT));
    lv_style_set_radius(&s_button_style, 8);
    lv_style_set_pad_all(&s_button_style, 0);

    lv_style_init(&s_button_active_style);
    lv_style_set_bg_color(&s_button_active_style, lv_color_hex(UI_ACCENT));
    lv_style_set_bg_opa(&s_button_active_style, LV_OPA_COVER);
    lv_style_set_border_width(&s_button_active_style, 2);
    lv_style_set_border_color(&s_button_active_style, lv_color_hex(UI_ACCENT));
    lv_style_set_radius(&s_button_active_style, 8);
    lv_style_set_pad_all(&s_button_active_style, 0);

    lv_style_init(&s_pill_style);
    lv_style_set_bg_opa(&s_pill_style, LV_OPA_TRANSP);
    lv_style_set_border_width(&s_pill_style, 2);
    lv_style_set_border_color(&s_pill_style, lv_color_hex(UI_ACCENT));
    lv_style_set_radius(&s_pill_style, 14);
    lv_style_set_pad_all(&s_pill_style, 0);

    lv_style_init(&s_nav_style);
    lv_style_set_bg_opa(&s_nav_style, LV_OPA_TRANSP);
    lv_style_set_border_width(&s_nav_style, 0);
    lv_style_set_radius(&s_nav_style, 8);
    lv_style_set_pad_all(&s_nav_style, 0);

    lv_style_init(&s_nav_active_style);
    lv_style_set_bg_color(&s_nav_active_style, lv_color_hex(UI_ACCENT));
    lv_style_set_bg_opa(&s_nav_active_style, LV_OPA_COVER);
    lv_style_set_border_width(&s_nav_active_style, 0);
    lv_style_set_radius(&s_nav_active_style, 8);
    lv_style_set_pad_all(&s_nav_active_style, 0);

    s_styles_ready = true;
}

static void clear_dynamic_refs(void)
{
    s_ui.time_label = NULL;
    s_ui.date_label = NULL;
    s_ui.standby_status_label = NULL;
    s_ui.standby_detail_label = NULL;
    s_ui.wifi_pill = NULL;
    s_ui.light_value = NULL;
    s_ui.fan_value = NULL;
    s_ui.temp_value = NULL;
    s_ui.humidity_value = NULL;
    s_ui.lux_value = NULL;
    s_ui.air_value = NULL;
    s_ui.comfort_value = NULL;
    s_ui.feedback_label = NULL;
}

static lv_obj_t *create_screen(void)
{
    clear_dynamic_refs();
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_size(screen, UI_W, UI_H);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_style(screen, &s_screen_style, LV_PART_MAIN | LV_STATE_DEFAULT);
    return screen;
}

static lv_obj_t *create_obj(lv_obj_t *parent, int x, int y, int w, int h, lv_style_t *style)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
    if (style != NULL) {
        lv_obj_add_style(obj, style, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    return obj;
}

static lv_obj_t *create_card(lv_obj_t *parent, int x, int y, int w, int h, bool accent)
{
    return create_obj(parent, x, y, w, h, accent ? &s_card_accent_style : &s_card_style);
}

static lv_obj_t *create_label(lv_obj_t *parent, const char *text, int x, int y, int w, int h,
                              const lv_font_t *font, uint32_t color, lv_text_align_t align)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_size(label, w, h);
    lv_label_set_text(label, text);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(label, align, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    return label;
}

static lv_obj_t *create_pill(lv_obj_t *parent, const char *text, int x, int y, int w)
{
    lv_obj_t *pill = create_obj(parent, x, y, w, 28, &s_pill_style);
    create_label(pill, text, 0, 5, w, 18, &lv_font_montserrat_14, UI_ACCENT, LV_TEXT_ALIGN_CENTER);
    return pill;
}

static lv_obj_t *create_button(lv_obj_t *parent, const char *text, int x, int y, int w, int h,
                               bool active, lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *button = create_obj(parent, x, y, w, h, active ? &s_button_active_style : &s_button_style);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    create_label(button, text, 0, (h - 22) / 2, w, 22, font_cn_16(),
                 active ? UI_BG : UI_ACCENT, LV_TEXT_ALIGN_CENTER);
    if (cb != NULL) {
        lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, user_data);
    }
    return button;
}

static lv_obj_t *create_meter(lv_obj_t *parent, int x, int y, int w, int value)
{
    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_set_pos(bar, x, y);
    lv_obj_set_size(bar, w, 12);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, value, LV_ANIM_OFF);
    lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(bar, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(bar, lv_color_hex(UI_MUTED), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(bar, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(bar, lv_color_hex(UI_ACCENT), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(bar, 4, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    return bar;
}

static lv_obj_t *create_switch_view(lv_obj_t *parent, int x, int y, bool on, lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *track = create_obj(parent, x, y, 86, 42, NULL);
    lv_obj_add_flag(track, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(track, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(track, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(track, lv_color_hex(on ? UI_ACCENT : UI_MUTED), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(track, 21, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *knob = lv_obj_create(track);
    lv_obj_set_pos(knob, on ? 48 : 6, 6);
    lv_obj_set_size(knob, 26, 26);
    lv_obj_clear_flag(knob, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(knob, 13, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(knob, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(knob, lv_color_hex(on ? UI_ACCENT : UI_MUTED), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(knob, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (cb != NULL) {
        lv_obj_add_event_cb(track, cb, LV_EVENT_CLICKED, user_data);
    }
    return track;
}

static void nav_event_cb(lv_event_t *event)
{
    ui_page_t page = (ui_page_t)(intptr_t)lv_event_get_user_data(event);
    ui_open_page(page);
}

static void light_event_cb(lv_event_t *event)
{
    (void)event;
    ui_state_toggle_light(&s_ui.state, UI_SOURCE_GUI);
    ui_open_page(UI_PAGE_CONTROL);
}

static void fan_event_cb(lv_event_t *event)
{
    (void)event;
    ui_state_toggle_fan(&s_ui.state, UI_SOURCE_GUI);
    ui_open_page(UI_PAGE_CONTROL);
}

static void scene_event_cb(lv_event_t *event)
{
    ui_scene_t scene = (ui_scene_t)(intptr_t)lv_event_get_user_data(event);
    ui_state_apply_scene(&s_ui.state, scene, UI_SOURCE_GUI);
    ui_open_page(UI_PAGE_SCENE);
}

static void ai_event_cb(lv_event_t *event)
{
    (void)event;
    ui_state_apply_ai_suggestion(&s_ui.state);
    ui_open_page(UI_PAGE_AI);
}

static void setting_event_cb(lv_event_t *event)
{
    int command = (int)(intptr_t)lv_event_get_user_data(event);
    if (command == 1) {
        ui_state_adjust_volume(&s_ui.state, 5);
    } else if (command == -1) {
        ui_state_adjust_volume(&s_ui.state, -5);
    } else if (command == 2) {
        ui_state_adjust_brightness(&s_ui.state, 5);
    } else if (command == -2) {
        ui_state_adjust_brightness(&s_ui.state, -5);
    }
    ui_open_page(UI_PAGE_SETTINGS);
}

static void create_topbar(lv_obj_t *screen, const char *title, const char *subtitle, const char *status)
{
    lv_obj_t *logo = create_obj(screen, 32, 22, 44, 44, &s_pill_style);
    lv_obj_set_style_radius(logo, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    create_label(logo, "P4", 0, 10, 44, 22, &lv_font_montserrat_18, UI_ACCENT, LV_TEXT_ALIGN_CENTER);
    create_label(screen, title, 90, 22, 330, 28, font_cn_24(), UI_INK, LV_TEXT_ALIGN_LEFT);
    create_label(screen, subtitle, 90, 54, 330, 18, &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);
    s_ui.wifi_pill = create_pill(screen, status, 560, 24, 126);
    lv_obj_t *line = create_obj(screen, 32, 82, 656, 2, NULL);
    lv_obj_set_style_bg_color(line, lv_color_hex(UI_MUTED), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(line, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
}

static void create_navbar(lv_obj_t *screen, ui_page_t active)
{
    static const char *labels[UI_PAGE_COUNT] = {"HOME", "CTRL", "ENV", "SCENE", "AI", "SET"};

    lv_obj_t *line = create_obj(screen, 32, 638, 656, 2, NULL);
    lv_obj_set_style_bg_color(line, lv_color_hex(UI_MUTED), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(line, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    for (int i = 0; i < UI_PAGE_COUNT; ++i) {
        int x = 34 + i * 109;
        bool selected = (ui_page_t)i == active;
        lv_obj_t *nav = create_obj(screen, x, 654, 94, 42, selected ? &s_nav_active_style : &s_nav_style);
        lv_obj_add_flag(nav, LV_OBJ_FLAG_CLICKABLE);
        create_label(nav, labels[i], 0, 12, 94, 18, &lv_font_montserrat_14,
                     selected ? UI_BG : UI_MUTED, LV_TEXT_ALIGN_CENTER);
        lv_obj_add_event_cb(nav, nav_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
}

static void refresh_dynamic_labels(void)
{
    char buffer[48];

    if (s_ui.time_label != NULL) {
        lv_label_set_text(s_ui.time_label, s_ui.state.time_text);
    }
    if (s_ui.date_label != NULL) {
        lv_label_set_text(s_ui.date_label, s_ui.state.date_text);
    }
    if (s_ui.standby_status_label != NULL) {
        const char *status = "Connecting network";
        if (s_ui.state.time_synced) {
            status = "Time ready";
        } else if (s_ui.state.wifi_connected) {
            status = "Syncing time";
        }
        lv_label_set_text(s_ui.standby_status_label, status);
    }
    if (s_ui.standby_detail_label != NULL) {
        const char *detail = "Starting Wi-Fi remote and waiting for SNTP.";
        if (s_ui.state.time_synced) {
            detail = "Opening HOME.";
        } else if (s_ui.state.wifi_connected) {
            detail = "Network online. Waiting for accurate local time.";
        }
        lv_label_set_text(s_ui.standby_detail_label, detail);
    }
    if (s_ui.wifi_pill != NULL) {
        lv_obj_t *label = lv_obj_get_child(s_ui.wifi_pill, 0);
        if (label != NULL) {
            lv_label_set_text(label, s_ui.state.wifi_connected ? "ONLINE" : "LOCAL");
        }
    }
    if (s_ui.light_value != NULL) {
        lv_label_set_text(s_ui.light_value, ui_state_onoff(s_ui.state.relay_light_on));
    }
    if (s_ui.fan_value != NULL) {
        lv_label_set_text(s_ui.fan_value, ui_state_onoff(s_ui.state.relay_fan_on));
    }
    if (s_ui.temp_value != NULL) {
        snprintf(buffer, sizeof(buffer), "%.1f C", s_ui.state.temperature);
        lv_label_set_text(s_ui.temp_value, buffer);
    }
    if (s_ui.humidity_value != NULL) {
        snprintf(buffer, sizeof(buffer), "%d %%", s_ui.state.humidity);
        lv_label_set_text(s_ui.humidity_value, buffer);
    }
    if (s_ui.lux_value != NULL) {
        snprintf(buffer, sizeof(buffer), "%d lx", s_ui.state.light_lux);
        lv_label_set_text(s_ui.lux_value, buffer);
    }
    if (s_ui.air_value != NULL) {
        snprintf(buffer, sizeof(buffer), "%d", s_ui.state.air_quality);
        lv_label_set_text(s_ui.air_value, buffer);
    }
    if (s_ui.comfort_value != NULL) {
        snprintf(buffer, sizeof(buffer), "%d", s_ui.state.comfort_score);
        lv_label_set_text(s_ui.comfort_value, buffer);
    }
    if (s_ui.feedback_label != NULL) {
        lv_label_set_text(s_ui.feedback_label, s_ui.state.last_feedback);
    }
}

static void create_footer_feedback(lv_obj_t *screen)
{
    lv_obj_t *card = create_card(screen, 32, 566, 656, 48, false);
    create_label(card, "Feedback", 18, 15, 94, 18, &lv_font_montserrat_14, UI_ACCENT, LV_TEXT_ALIGN_LEFT);
    s_ui.feedback_label = create_label(card, s_ui.state.last_feedback, 118, 15, 510, 18,
                                       &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);
}

static lv_obj_t *build_standby_page(void)
{
    lv_obj_t *screen = create_screen();
    create_topbar(screen, "Starting", "NETWORK / SNTP / LOCAL TIME", s_ui.state.wifi_connected ? "ONLINE" : "LOCAL");

    lv_obj_t *panel = create_card(screen, 110, 202, 500, 276, true);
    create_label(panel, "P4", 0, 44, 500, 50, &lv_font_montserrat_44, UI_ACCENT, LV_TEXT_ALIGN_CENTER);
    s_ui.standby_status_label = create_label(panel, "Connecting network", 0, 126, 500, 32,
                                             &lv_font_montserrat_24, UI_INK, LV_TEXT_ALIGN_CENTER);
    s_ui.standby_detail_label = create_label(panel, "Starting Wi-Fi remote and waiting for SNTP.",
                                             50, 176, 400, 20, &lv_font_montserrat_14,
                                             UI_MUTED, LV_TEXT_ALIGN_CENTER);

    create_pill(panel, "BOOT", 100, 224, 84);
    create_pill(panel, "WIFI", 208, 224, 84);
    create_pill(panel, "TIME", 316, 224, 84);

    lv_obj_t *line = create_obj(screen, 144, 536, 432, 2, NULL);
    lv_obj_set_style_bg_color(line, lv_color_hex(UI_MUTED), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(line, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    refresh_dynamic_labels();
    return screen;
}

static lv_obj_t *build_home_page(void)
{
    lv_obj_t *screen = create_screen();
    create_topbar(screen, "HOME", "STATUS / QUICK ACTIONS", s_ui.state.wifi_connected ? "ONLINE" : "LOCAL");

    lv_obj_t *time_card = create_card(screen, 32, 112, 314, 150, true);
    create_label(time_card, "LOCAL TIME", 24, 18, 150, 18, &lv_font_montserrat_14, UI_ACCENT, LV_TEXT_ALIGN_LEFT);
    s_ui.time_label = create_label(time_card, s_ui.state.time_text, 24, 48, 210, 50,
                                   &lv_font_montserrat_44, UI_INK, LV_TEXT_ALIGN_LEFT);
    s_ui.date_label = create_label(time_card, s_ui.state.date_text, 24, 106, 210, 18,
                                   &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);

    lv_obj_t *assistant = create_card(screen, 370, 112, 318, 150, false);
    create_label(assistant, "Assistant", 24, 22, 160, 22, &lv_font_montserrat_18, UI_INK, LV_TEXT_ALIGN_LEFT);
    create_label(assistant, s_ui.state.voice_ready ? "Voice ready / Touch active" : "Touch active",
                 24, 58, 230, 18, &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);
    create_pill(assistant, "MIC", 24, 102, 66);
    create_pill(assistant, "LLM", 106, 102, 66);
    create_pill(assistant, "TTS", 188, 102, 66);

    lv_obj_t *temp = create_card(screen, 32, 286, 150, 112, false);
    create_label(temp, "Temp", 18, 18, 90, 18, &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);
    s_ui.temp_value = create_label(temp, "0.0 C", 18, 50, 112, 34, &lv_font_montserrat_30, UI_INK, LV_TEXT_ALIGN_LEFT);

    lv_obj_t *hum = create_card(screen, 198, 286, 150, 112, false);
    create_label(hum, "Humidity", 18, 18, 100, 18, &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);
    s_ui.humidity_value = create_label(hum, "0 %", 18, 50, 112, 34, &lv_font_montserrat_30, UI_INK, LV_TEXT_ALIGN_LEFT);

    lv_obj_t *lux = create_card(screen, 364, 286, 150, 112, false);
    create_label(lux, "Light", 18, 18, 90, 18, &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);
    s_ui.lux_value = create_label(lux, "0 lx", 18, 50, 112, 34, &lv_font_montserrat_30, UI_INK, LV_TEXT_ALIGN_LEFT);

    lv_obj_t *air = create_card(screen, 530, 286, 158, 112, false);
    create_label(air, "Air", 18, 18, 90, 18, &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);
    s_ui.air_value = create_label(air, "0", 18, 50, 112, 34, &lv_font_montserrat_30, UI_INK, LV_TEXT_ALIGN_LEFT);

    lv_obj_t *device = create_card(screen, 32, 424, 314, 118, false);
    create_label(device, "Device", 20, 18, 116, 22, &lv_font_montserrat_18, UI_INK, LV_TEXT_ALIGN_LEFT);
    create_label(device, "Light", 20, 58, 80, 18, &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);
    s_ui.light_value = create_label(device, ui_state_onoff(s_ui.state.relay_light_on), 224, 56, 56, 20,
                                    &lv_font_montserrat_14, UI_ACCENT, LV_TEXT_ALIGN_CENTER);
    create_label(device, "Vent", 20, 86, 80, 18, &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);
    s_ui.fan_value = create_label(device, ui_state_onoff(s_ui.state.relay_fan_on), 224, 84, 56, 20,
                                  &lv_font_montserrat_14, UI_ACCENT, LV_TEXT_ALIGN_CENTER);

    lv_obj_t *quick = create_card(screen, 370, 424, 318, 118, false);
    create_label(quick, "Quick scene", 20, 18, 160, 22, &lv_font_montserrat_18, UI_INK, LV_TEXT_ALIGN_LEFT);
    create_button(quick, "Study", 20, 58, 82, 42, true, scene_event_cb, (void *)(intptr_t)UI_SCENE_STUDY);
    create_button(quick, "Away", 118, 58, 82, 42, false, scene_event_cb, (void *)(intptr_t)UI_SCENE_AWAY);
    create_button(quick, "Auto", 216, 58, 76, 42, false, scene_event_cb, (void *)(intptr_t)UI_SCENE_AUTO);

    create_footer_feedback(screen);
    create_navbar(screen, UI_PAGE_HOME);
    refresh_dynamic_labels();
    return screen;
}

static lv_obj_t *build_control_page(void)
{
    lv_obj_t *screen = create_screen();
    create_topbar(screen, "Control", "RELAY / MANUAL CONTROL", ui_state_scene_name(s_ui.state.scene));

    lv_obj_t *light = create_card(screen, 32, 112, 314, 232, true);
    create_label(light, "Light relay", 26, 24, 160, 24, &lv_font_montserrat_18, UI_INK, LV_TEXT_ALIGN_LEFT);
    create_label(light, "Relay 1 / Lamp", 26, 62, 160, 18, &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);
    s_ui.light_value = create_label(light, ui_state_onoff(s_ui.state.relay_light_on), 26, 108, 130, 40,
                                    &lv_font_montserrat_32, UI_INK, LV_TEXT_ALIGN_LEFT);
    create_switch_view(light, 198, 108, s_ui.state.relay_light_on, light_event_cb, NULL);
    create_label(light, ui_state_source_name(s_ui.state.source), 26, 178, 220, 18,
                 &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);

    lv_obj_t *fan = create_card(screen, 374, 112, 314, 232, false);
    create_label(fan, "Vent relay", 26, 24, 160, 24, &lv_font_montserrat_18, UI_INK, LV_TEXT_ALIGN_LEFT);
    create_label(fan, "Relay 2 / Fan", 26, 62, 160, 18, &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);
    s_ui.fan_value = create_label(fan, ui_state_onoff(s_ui.state.relay_fan_on), 26, 108, 130, 40,
                                  &lv_font_montserrat_32, UI_INK, LV_TEXT_ALIGN_LEFT);
    create_switch_view(fan, 198, 108, s_ui.state.relay_fan_on, fan_event_cb, NULL);
    create_label(fan, ui_state_source_name(s_ui.state.source), 26, 178, 220, 18,
                 &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);

    lv_obj_t *feedback = create_card(screen, 32, 372, 656, 98, false);
    create_label(feedback, "Execution", 24, 22, 140, 22, &lv_font_montserrat_18, UI_INK, LV_TEXT_ALIGN_LEFT);
    s_ui.feedback_label = create_label(feedback, s_ui.state.last_feedback, 24, 58, 560, 20,
                                       &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);

    create_button(screen, "Study", 32, 500, 198, 46, true, scene_event_cb, (void *)(intptr_t)UI_SCENE_STUDY);
    create_button(screen, "Away", 260, 500, 198, 46, false, scene_event_cb, (void *)(intptr_t)UI_SCENE_AWAY);
    create_button(screen, "AI advice", 490, 500, 198, 46, false, ai_event_cb, NULL);

    create_navbar(screen, UI_PAGE_CONTROL);
    refresh_dynamic_labels();
    return screen;
}

static lv_obj_t *build_env_page(void)
{
    lv_obj_t *screen = create_screen();
    create_topbar(screen, "Environment", "SENSOR / COMFORT", s_ui.state.sensor_online ? "SENSOR" : "LOCAL");

    lv_obj_t *comfort = create_card(screen, 32, 112, 656, 112, true);
    create_label(comfort, "Comfort score", 26, 22, 180, 22, &lv_font_montserrat_18, UI_INK, LV_TEXT_ALIGN_LEFT);
    s_ui.comfort_value = create_label(comfort, "0", 26, 54, 70, 40, &lv_font_montserrat_32, UI_INK, LV_TEXT_ALIGN_LEFT);
    create_label(comfort, "Local rule result for study environment", 112, 64, 360, 18,
                 &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);
    create_pill(comfort, s_ui.state.comfort_score > 75 ? "GOOD" : "CHECK", 538, 42, 76);

    lv_obj_t *temp = create_card(screen, 32, 250, 314, 130, false);
    create_label(temp, "Temperature / Humidity", 22, 22, 220, 20, &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);
    s_ui.temp_value = create_label(temp, "0.0 C", 22, 54, 130, 34, &lv_font_montserrat_30, UI_INK, LV_TEXT_ALIGN_LEFT);
    s_ui.humidity_value = create_label(temp, "0 %", 170, 54, 110, 34, &lv_font_montserrat_30, UI_INK, LV_TEXT_ALIGN_LEFT);
    create_meter(temp, 22, 104, 250, s_ui.state.humidity);

    lv_obj_t *light = create_card(screen, 374, 250, 314, 130, false);
    create_label(light, "Light level", 22, 22, 220, 20, &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);
    s_ui.lux_value = create_label(light, "0 lx", 22, 54, 160, 34, &lv_font_montserrat_30, UI_INK, LV_TEXT_ALIGN_LEFT);
    create_meter(light, 22, 104, 250, s_ui.state.light_lux > 600 ? 100 : s_ui.state.light_lux / 6);

    lv_obj_t *air = create_card(screen, 32, 408, 314, 130, false);
    create_label(air, "Air quality", 22, 22, 220, 20, &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);
    s_ui.air_value = create_label(air, "0", 22, 54, 160, 34, &lv_font_montserrat_30, UI_INK, LV_TEXT_ALIGN_LEFT);
    create_meter(air, 22, 104, 250, s_ui.state.air_quality);

    lv_obj_t *sensor = create_card(screen, 374, 408, 314, 130, false);
    create_label(sensor, "Sensor link", 22, 22, 220, 20, &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);
    create_label(sensor, s_ui.state.sensor_online ? "ONLINE" : "SIM DATA", 22, 58, 160, 28,
                 &lv_font_montserrat_24, UI_INK, LV_TEXT_ALIGN_LEFT);
    create_label(sensor, "RS485 target / 1s refresh", 22, 100, 220, 18,
                 &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);

    create_navbar(screen, UI_PAGE_ENV);
    refresh_dynamic_labels();
    return screen;
}

static lv_obj_t *build_scene_page(void)
{
    lv_obj_t *screen = create_screen();
    create_topbar(screen, "Scene Mode", "SCENE / TIMER / AUTO RULE", ui_state_scene_name(s_ui.state.scene));

    lv_obj_t *current = create_card(screen, 32, 112, 656, 108, true);
    create_label(current, "Current scene", 26, 22, 180, 22, &lv_font_montserrat_18, UI_INK, LV_TEXT_ALIGN_LEFT);
    create_label(current, ui_state_scene_name(s_ui.state.scene), 26, 56, 180, 34,
                 &lv_font_montserrat_30, UI_INK, LV_TEXT_ALIGN_LEFT);
    create_label(current, "Scene updates relays and feeds HOME/control pages.", 242, 60, 360, 18,
                 &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);

    create_button(screen, "Study", 32, 250, 198, 76, s_ui.state.scene == UI_SCENE_STUDY,
                  scene_event_cb, (void *)(intptr_t)UI_SCENE_STUDY);
    create_button(screen, "Rest", 260, 250, 198, 76, s_ui.state.scene == UI_SCENE_REST,
                  scene_event_cb, (void *)(intptr_t)UI_SCENE_REST);
    create_button(screen, "Away", 490, 250, 198, 76, s_ui.state.scene == UI_SCENE_AWAY,
                  scene_event_cb, (void *)(intptr_t)UI_SCENE_AWAY);
    create_button(screen, "Auto", 32, 352, 198, 76, s_ui.state.scene == UI_SCENE_AUTO,
                  scene_event_cb, (void *)(intptr_t)UI_SCENE_AUTO);
    create_button(screen, "Manual", 260, 352, 198, 76, s_ui.state.scene == UI_SCENE_MANUAL,
                  scene_event_cb, (void *)(intptr_t)UI_SCENE_MANUAL);
    create_button(screen, "Timer", 490, 352, 198, 76, s_ui.state.scene == UI_SCENE_TIMER,
                  scene_event_cb, (void *)(intptr_t)UI_SCENE_TIMER);

    lv_obj_t *preview = create_card(screen, 32, 462, 656, 82, false);
    create_label(preview, "Action preview", 24, 18, 160, 20, &lv_font_montserrat_18, UI_INK, LV_TEXT_ALIGN_LEFT);
    create_label(preview, s_ui.state.last_feedback, 24, 48, 560, 18, &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);

    create_navbar(screen, UI_PAGE_SCENE);
    return screen;
}

static lv_obj_t *build_ai_page(void)
{
    lv_obj_t *screen = create_screen();
    create_topbar(screen, "AI Assistant", "ASR / LLM / COMMAND CHECK", s_ui.state.ai_connected ? "READY" : "LOCAL");

    lv_obj_t *user = create_card(screen, 32, 112, 656, 86, false);
    create_label(user, "User voice", 24, 16, 130, 18, &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);
    create_label(user, s_ui.state.ai_user_text, 24, 44, 580, 22, &lv_font_montserrat_18, UI_INK, LV_TEXT_ALIGN_LEFT);

    lv_obj_t *reply = create_card(screen, 32, 222, 656, 128, true);
    create_label(reply, "LLM reply", 24, 18, 130, 18, &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);
    create_label(reply, s_ui.state.ai_reply_text, 24, 48, 580, 52, &lv_font_montserrat_18, UI_INK, LV_TEXT_ALIGN_LEFT);

    lv_obj_t *strategy = create_card(screen, 32, 378, 314, 146, false);
    create_label(strategy, "Strategy", 24, 20, 140, 22, &lv_font_montserrat_18, UI_INK, LV_TEXT_ALIGN_LEFT);
    create_label(strategy, "relay_1: ON", 24, 62, 140, 18, &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);
    create_label(strategy, s_ui.state.temperature > 28.0f ? "relay_2: ON" : "relay_2: KEEP",
                 24, 92, 160, 18, &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);

    lv_obj_t *check = create_card(screen, 374, 378, 314, 146, false);
    create_label(check, "Safety check", 24, 20, 160, 22, &lv_font_montserrat_18, UI_INK, LV_TEXT_ALIGN_LEFT);
    create_label(check, "Whitelist: PASS", 24, 62, 180, 18, &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);
    create_label(check, "Relay interval: PASS", 24, 92, 200, 18, &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);
    create_pill(check, "SAFE", 218, 78, 70);

    create_button(screen, "Apply AI", 260, 542, 198, 46, true, ai_event_cb, NULL);
    create_navbar(screen, UI_PAGE_AI);
    return screen;
}

static lv_obj_t *build_settings_page(void)
{
    char buffer[48];
    lv_obj_t *screen = create_screen();
    create_topbar(screen, "Settings", "NETWORK / AUDIO / DEVICE", "SET");

    lv_obj_t *network = create_card(screen, 32, 112, 314, 128, true);
    create_label(network, "Network", 22, 20, 130, 22, &lv_font_montserrat_18, UI_INK, LV_TEXT_ALIGN_LEFT);
    create_label(network, s_ui.state.wifi_connected ? "Wi-Fi connected" : "Offline local mode",
                 22, 58, 220, 18, &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);
    create_pill(network, s_ui.state.wifi_connected ? "OK" : "LOCAL", 218, 20, 70);

    lv_obj_t *audio = create_card(screen, 374, 112, 314, 128, false);
    create_label(audio, "Audio", 22, 20, 130, 22, &lv_font_montserrat_18, UI_INK, LV_TEXT_ALIGN_LEFT);
    snprintf(buffer, sizeof(buffer), "Volume %d%%", s_ui.state.volume);
    create_label(audio, buffer, 22, 58, 130, 18, &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);
    create_button(audio, "-", 178, 48, 44, 36, false, setting_event_cb, (void *)(intptr_t)-1);
    create_button(audio, "+", 236, 48, 44, 36, false, setting_event_cb, (void *)(intptr_t)1);

    lv_obj_t *screen_cfg = create_card(screen, 32, 266, 314, 128, false);
    create_label(screen_cfg, "Screen", 22, 20, 130, 22, &lv_font_montserrat_18, UI_INK, LV_TEXT_ALIGN_LEFT);
    snprintf(buffer, sizeof(buffer), "Brightness %d%%", s_ui.state.screen_brightness);
    create_label(screen_cfg, buffer, 22, 58, 150, 18, &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);
    create_button(screen_cfg, "-", 178, 48, 44, 36, false, setting_event_cb, (void *)(intptr_t)-2);
    create_button(screen_cfg, "+", 236, 48, 44, 36, false, setting_event_cb, (void *)(intptr_t)2);

    lv_obj_t *rs485 = create_card(screen, 374, 266, 314, 128, false);
    create_label(rs485, "RS485", 22, 20, 130, 22, &lv_font_montserrat_18, UI_INK, LV_TEXT_ALIGN_LEFT);
    create_label(rs485, "9600 / 8N1 / addr 01", 22, 58, 220, 18,
                 &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);
    create_pill(rs485, "MODBUS", 198, 20, 90);

    lv_obj_t *info = create_card(screen, 32, 420, 656, 112, false);
    create_label(info, "System", 22, 20, 130, 22, &lv_font_montserrat_18, UI_INK, LV_TEXT_ALIGN_LEFT);
    create_label(info, "ESP32-P4 / LVGL / 720x720 / PSRAM double buffer", 22, 58, 520, 18,
                 &lv_font_montserrat_14, UI_MUTED, LV_TEXT_ALIGN_LEFT);
    create_pill(info, "OTA", 548, 42, 70);

    create_navbar(screen, UI_PAGE_SETTINGS);
    return screen;
}

static lv_obj_t *build_page(ui_page_t page)
{
    switch (page) {
    case UI_PAGE_CONTROL:
        return build_control_page();
    case UI_PAGE_ENV:
        return build_env_page();
    case UI_PAGE_SCENE:
        return build_scene_page();
    case UI_PAGE_AI:
        return build_ai_page();
    case UI_PAGE_SETTINGS:
        return build_settings_page();
    case UI_PAGE_HOME:
    default:
        return build_home_page();
    }
}

static void ui_open_page(ui_page_t page)
{
    if (page >= UI_PAGE_COUNT) {
        return;
    }

    ui_state_poll_external(&s_ui.state);
    lv_obj_t *new_screen = build_page(page);
    bool auto_delete_old = s_ui.screen != NULL;

    lv_scr_load_anim(new_screen, LV_SCR_LOAD_ANIM_FADE_ON, 120, 0, auto_delete_old);
    s_ui.screen = new_screen;
    s_ui.current_page = page;
}

static void ui_open_standby_page(void)
{
    lv_obj_t *new_screen = build_standby_page();
    bool auto_delete_old = s_ui.screen != NULL;

    lv_scr_load_anim(new_screen, LV_SCR_LOAD_ANIM_FADE_ON, 120, 0, auto_delete_old);
    s_ui.screen = new_screen;
}

static void ui_tick_cb(lv_timer_t *timer)
{
    (void)timer;
    ui_state_poll_external(&s_ui.state);
    if (s_ui.waiting_for_time && s_ui.state.time_synced) {
        s_ui.waiting_for_time = false;
        ui_open_page(UI_PAGE_HOME);
        return;
    }
    refresh_dynamic_labels();
}

esp_err_t ui_start(void)
{
    style_init_once();
    ui_state_init(&s_ui.state);
    ui_state_poll_external(&s_ui.state);
    s_ui.waiting_for_time = !s_ui.state.time_synced;
    if (s_ui.waiting_for_time) {
        ui_open_standby_page();
    } else {
        ui_open_page(UI_PAGE_HOME);
    }

    if (s_ui.timer == NULL) {
        s_ui.timer = lv_timer_create(ui_tick_cb, 1000, NULL);
        if (s_ui.timer == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    return ESP_OK;
}
