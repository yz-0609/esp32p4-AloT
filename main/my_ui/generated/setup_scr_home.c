#include "lvgl.h"
#include <stdio.h>
#include "gui_guider.h"
#include "events_init.h"
#include "widgets_init.h"
#include "custom.h"
#include "app_wifi.h"

#define UI_BG_COLOR      0xF8FAFC
#define UI_INK_COLOR     0x0F172A
#define UI_LINE_COLOR    0xE2E8F0
#define UI_ACCENT_COLOR  0x38BDF8
#define UI_MUTED_COLOR   0x64748B

static lv_style_t s_style_screen;
static lv_style_t s_style_frame;
static lv_style_t s_style_card;
static lv_style_t s_style_icon_box;
static lv_style_t s_style_pill;
static bool s_styles_inited;

static void init_aiot_styles(void)
{
    if (s_styles_inited) {
        return;
    }

    lv_style_init(&s_style_screen);
    lv_style_set_bg_color(&s_style_screen, lv_color_hex(UI_BG_COLOR));
    lv_style_set_bg_opa(&s_style_screen, LV_OPA_COVER);
    lv_style_set_border_width(&s_style_screen, 1);
    lv_style_set_border_color(&s_style_screen, lv_color_hex(UI_LINE_COLOR));
    lv_style_set_radius(&s_style_screen, 0);
    lv_style_set_pad_all(&s_style_screen, 0);

    lv_style_init(&s_style_frame);
    lv_style_set_bg_opa(&s_style_frame, LV_OPA_TRANSP);
    lv_style_set_border_width(&s_style_frame, 1);
    lv_style_set_border_color(&s_style_frame, lv_color_hex(UI_LINE_COLOR));
    lv_style_set_radius(&s_style_frame, 26);
    lv_style_set_pad_all(&s_style_frame, 0);

    lv_style_init(&s_style_card);
    lv_style_set_bg_color(&s_style_card, lv_color_hex(UI_BG_COLOR));
    lv_style_set_bg_opa(&s_style_card, LV_OPA_COVER);
    lv_style_set_border_width(&s_style_card, 1);
    lv_style_set_border_color(&s_style_card, lv_color_hex(UI_LINE_COLOR));
    lv_style_set_radius(&s_style_card, 24);
    lv_style_set_pad_all(&s_style_card, 0);

    lv_style_init(&s_style_icon_box);
    lv_style_set_bg_opa(&s_style_icon_box, LV_OPA_TRANSP);
    lv_style_set_border_width(&s_style_icon_box, 1);
    lv_style_set_border_color(&s_style_icon_box, lv_color_hex(UI_ACCENT_COLOR));
    lv_style_set_radius(&s_style_icon_box, 14);
    lv_style_set_pad_all(&s_style_icon_box, 0);

    lv_style_init(&s_style_pill);
    lv_style_set_bg_opa(&s_style_pill, LV_OPA_TRANSP);
    lv_style_set_border_width(&s_style_pill, 1);
    lv_style_set_border_color(&s_style_pill, lv_color_hex(UI_ACCENT_COLOR));
    lv_style_set_radius(&s_style_pill, 16);
    lv_style_set_pad_left(&s_style_pill, 8);
    lv_style_set_pad_right(&s_style_pill, 8);
    lv_style_set_pad_top(&s_style_pill, 4);
    lv_style_set_pad_bottom(&s_style_pill, 4);

    s_styles_inited = true;
}

static lv_obj_t *create_obj(lv_obj_t *parent, int x, int y, int w, int h, lv_style_t *style)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
    if (style) {
        lv_obj_add_style(obj, style, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    return obj;
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

static lv_obj_t *create_icon_label(lv_obj_t *parent, const char *text, int x, int y, int w, int h)
{
    lv_obj_t *box = create_obj(parent, x, y, w, h, &s_style_icon_box);
    create_label(box, text, 0, (h - 18) / 2, w, 20, &lv_font_montserrat_18, UI_ACCENT_COLOR, LV_TEXT_ALIGN_CENTER);
    return box;
}

static lv_obj_t *create_pill(lv_obj_t *parent, const char *text, int x, int y, int w, int h)
{
    lv_obj_t *pill = create_obj(parent, x, y, w, h, &s_style_pill);
    create_label(pill, text, 0, 5, w, 14, &lv_font_montserrat_12, UI_ACCENT_COLOR, LV_TEXT_ALIGN_CENTER);
    return pill;
}

static void create_flow_card(lv_obj_t *parent, int x, const char *title, const char *line1, const char *line2)
{
    lv_obj_t *card = create_obj(parent, x, 362, 152, 132, &s_style_card);
    lv_obj_t *line = lv_obj_create(card);
    lv_obj_set_pos(line, 18, 18);
    lv_obj_set_size(line, 38, 6);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(line, lv_color_hex(UI_ACCENT_COLOR), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(line, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(line, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(line, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    create_label(card, title, 18, 48, 116, 24, &lv_font_simhei_24, UI_INK_COLOR, LV_TEXT_ALIGN_LEFT);
    create_label(card, line1, 18, 86, 116, 17, &lv_font_simhei_16, UI_MUTED_COLOR, LV_TEXT_ALIGN_LEFT);
    create_label(card, line2, 18, 106, 116, 17, &lv_font_simhei_16, UI_MUTED_COLOR, LV_TEXT_ALIGN_LEFT);
}

static void create_scene(lv_obj_t *parent, int y, const char *icon, const char *title,
                         const char *desc, const char *state)
{
    lv_obj_t *scene = create_obj(parent, 18, y, 378, 46, &s_style_card);
    lv_obj_set_style_radius(scene, 18, LV_PART_MAIN | LV_STATE_DEFAULT);
    create_icon_label(scene, icon, 10, 5, 36, 36);
    create_label(scene, title, 58, 8, 160, 18, &lv_font_simhei_16, UI_INK_COLOR, LV_TEXT_ALIGN_LEFT);
    create_label(scene, desc, 58, 28, 205, 15, &lv_font_simhei_16, UI_MUTED_COLOR, LV_TEXT_ALIGN_LEFT);
    create_pill(scene, state, 296, 10, 64, 24);
}

static void update_network_time_labels(lv_ui *ui)
{
    if (ui == NULL || ui->time_label == NULL || ui->date_label == NULL) {
        return;
    }

    char time_text[16] = {0};
    char date_text[40] = {0};

    if (app_wifi_format_time(time_text, sizeof(time_text), "%H:%M:%S") == ESP_OK) {
        lv_label_set_text(ui->time_label, time_text);
    }

    if (app_wifi_format_time(date_text, sizeof(date_text), "%Y / %m / %d - %A") == ESP_OK) {
        lv_label_set_text(ui->date_label, date_text);
    }
}

static void update_standby_layer(lv_ui *ui)
{
    if (ui == NULL || ui->standby_layer == NULL) {
        return;
    }

    if (app_wifi_time_is_synced()) {
        lv_obj_del(ui->standby_layer);
        ui->standby_layer = NULL;
        ui->standby_status_label = NULL;
        return;
    }

    if (ui->standby_status_label == NULL) {
        return;
    }

    if (app_wifi_is_connected()) {
        lv_label_set_text(ui->standby_status_label, "正在同步网络时间...");
    } else {
        lv_label_set_text(ui->standby_status_label, "正在连接 Wi-Fi...");
    }
}

static void network_time_timer_cb(lv_timer_t *timer)
{
    lv_ui *ui = (lv_ui *)timer->user_data;
    update_network_time_labels(ui);
    update_standby_layer(ui);
}

static void create_standby_layer(lv_ui *ui)
{
    ui->standby_layer = lv_obj_create(ui->home);
    lv_obj_set_pos(ui->standby_layer, 0, 0);
    lv_obj_set_size(ui->standby_layer, 720, 720);
    lv_obj_clear_flag(ui->standby_layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(ui->standby_layer, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(ui->standby_layer, lv_color_hex(UI_BG_COLOR), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->standby_layer, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->standby_layer, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui->standby_layer, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    create_obj(ui->standby_layer, 18, 18, 684, 684, &s_style_frame);
    create_obj(ui->standby_layer, 310, 220, 100, 100, &s_style_icon_box);
    create_label(ui->standby_layer, "P4", 310, 244, 100, 52, &lv_font_montserrat_44, UI_ACCENT_COLOR, LV_TEXT_ALIGN_CENTER);
    create_label(ui->standby_layer, "AIoT 桌面交互中枢", 160, 352, 400, 28, &lv_font_simhei_24, UI_INK_COLOR, LV_TEXT_ALIGN_CENTER);
    ui->standby_status_label = create_label(ui->standby_layer, "正在连接 Wi-Fi...", 160, 398, 400, 22,
                                            &lv_font_simhei_16, UI_MUTED_COLOR, LV_TEXT_ALIGN_CENTER);
}

void setup_scr_home(lv_ui *ui)
{
    init_aiot_styles();

    ui->home = lv_obj_create(NULL);
    lv_obj_set_size(ui->home, 720, 720);
    lv_obj_clear_flag(ui->home, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(ui->home, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_style(ui->home, &s_style_screen, LV_PART_MAIN | LV_STATE_DEFAULT);

    create_obj(ui->home, 18, 18, 684, 684, &s_style_frame);

    create_obj(ui->home, 34, 34, 46, 46, &s_style_icon_box);
    create_label(ui->home, "P4", 34, 47, 46, 20, &lv_font_montserrat_18, UI_ACCENT_COLOR, LV_TEXT_ALIGN_CENTER);
    create_label(ui->home, "AIoT 桌面交互中枢", 94, 33, 330, 30, &lv_font_simhei_24, UI_INK_COLOR, LV_TEXT_ALIGN_LEFT);
    create_label(ui->home, "ESP32-P4 Multimodal Agent", 94, 66, 300, 16, &lv_font_montserrat_12, UI_MUTED_COLOR, LV_TEXT_ALIGN_LEFT);

    lv_obj_t *status = create_obj(ui->home, 550, 36, 136, 42, &s_style_pill);
    lv_obj_set_style_radius(status, 21, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_t *dot = lv_obj_create(status);
    lv_obj_set_pos(dot, 20, 15);
    lv_obj_set_size(dot, 10, 10);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(dot, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(dot, lv_color_hex(UI_ACCENT_COLOR), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    ui->status_label = create_label(status, "ONLINE", 40, 11, 76, 16, &lv_font_montserrat_14, UI_INK_COLOR, LV_TEXT_ALIGN_CENTER);

    lv_obj_t *time_card = create_obj(ui->home, 34, 124, 438, 172, &s_style_card);
    create_label(time_card, "LOCAL TIME", 26, 24, 160, 16, &lv_font_montserrat_14, UI_ACCENT_COLOR, LV_TEXT_ALIGN_LEFT);
    ui->time_label = create_label(time_card, "08:30:00", 26, 63, 320, 48, &lv_font_montserrat_44, UI_INK_COLOR, LV_TEXT_ALIGN_LEFT);
    ui->date_label = create_label(time_card, "2026 / 06 / 05 - Friday", 26, 132, 340, 18, &lv_font_montserrat_14, UI_MUTED_COLOR, LV_TEXT_ALIGN_LEFT);

    lv_obj_t *voice_card = create_obj(ui->home, 496, 124, 190, 172, &s_style_card);
    create_icon_label(voice_card, "MIC", 22, 22, 72, 72);
    ui->voice_title_label = create_label(voice_card, "语音待命", 22, 107, 146, 20, &lv_font_simhei_16, UI_INK_COLOR, LV_TEXT_ALIGN_LEFT);
    create_label(voice_card, "本地唤醒", 22, 131, 146, 16, &lv_font_simhei_16, UI_MUTED_COLOR, LV_TEXT_ALIGN_LEFT);
    create_label(voice_card, "云端理解 / TTS", 22, 151, 146, 16, &lv_font_simhei_16, UI_MUTED_COLOR, LV_TEXT_ALIGN_LEFT);

    create_label(ui->home, "系统链路", 34, 330, 140, 24, &lv_font_simhei_24, UI_INK_COLOR, LV_TEXT_ALIGN_LEFT);
    create_label(ui->home, "PERCEPTION TO SERVICE", 486, 335, 200, 16, &lv_font_montserrat_12, UI_ACCENT_COLOR, LV_TEXT_ALIGN_RIGHT);

    create_flow_card(ui->home, 34, "感知", "语音唤醒", "轻量检测");
    create_flow_card(ui->home, 200, "交互", "LVGL 界面", "触控反馈");
    create_flow_card(ui->home, 366, "连接", "MQTT 通讯", "上位机联动");
    create_flow_card(ui->home, 532, "服务", "LLM 网关", "场景执行");

    lv_obj_t *assistant = create_obj(ui->home, 34, 518, 220, 172, &s_style_card);
    create_label(assistant, "AGENT MODE", 22, 20, 120, 14, &lv_font_montserrat_12, UI_ACCENT_COLOR, LV_TEXT_ALIGN_LEFT);
    create_label(assistant, "一句话控制", 22, 48, 176, 24, &lv_font_simhei_24, UI_INK_COLOR, LV_TEXT_ALIGN_LEFT);
    create_label(assistant, "桌面与居家场景", 22, 76, 176, 24, &lv_font_simhei_24, UI_INK_COLOR, LV_TEXT_ALIGN_LEFT);
    create_label(assistant, "端侧交互 / 设备联动", 22, 126, 176, 18, &lv_font_simhei_16, UI_MUTED_COLOR, LV_TEXT_ALIGN_LEFT);
    create_label(assistant, "状态展示主界面", 22, 148, 176, 18, &lv_font_simhei_16, UI_MUTED_COLOR, LV_TEXT_ALIGN_LEFT);

    lv_obj_t *scene_list = create_obj(ui->home, 272, 518, 414, 172, &s_style_card);
    create_scene(scene_list, 12, "L", "桌面灯光", "亮度 68% / 自动模式", "ACTIVE");
    create_scene(scene_list, 66, "MQ", "MQTT 连接", "Broker 已连接 / 低延迟", "SYNC");
    create_scene(scene_list, 120, "AI", "大模型网关", "自然语言理解 / 分发", "READY");

    create_label(ui->home, "720 x 720 UI MOCKUP", 490, 696, 190, 12, &lv_font_montserrat_10, UI_MUTED_COLOR, LV_TEXT_ALIGN_RIGHT);

    create_standby_layer(ui);

    lv_obj_update_layout(ui->home);
    update_network_time_labels(ui);
    update_standby_layer(ui);
    lv_timer_create(network_time_timer_cb, 1000, ui);
    events_init_home(ui);
}
