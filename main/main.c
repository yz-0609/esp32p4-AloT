#include "nvs_flash.h"
#include "esp_err.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "lvgl.h"
#include "app_relay.h"
#include "app_wifi.h"
#include "app_weather.h"
#include "ui.h"

static void app_nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

void app_main(void)
{
    app_nvs_init();
    ESP_ERROR_CHECK(app_relay_init());

    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_H_RES * BSP_LCD_V_RES,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .flags = {
            .buff_dma = false,
            .buff_spiram = true,
            .sw_rotate = true,
        }
    };
    lv_display_t *disp = bsp_display_start_with_config(&cfg);
    (void)disp;

    // if (disp != NULL)
    // {
    //     bsp_display_rotate(disp, LV_DISPLAY_ROTATION_90); // 90、180、270
    // }

    bsp_display_backlight_on();

    bsp_display_lock(0);

    ESP_ERROR_CHECK(ui_start());

    bsp_display_unlock();

    ESP_ERROR_CHECK(app_wifi_start());
    ESP_ERROR_CHECK(app_weather_start());
}
