file(GLOB_RECURSE UI_SRCS
    "ui/*.c"
)

idf_component_register(
    SRCS
        main.c
        app_microphone.c
        app_relay.c
        app_wifi.c
        app_weather.c
        ${UI_SRCS}
    INCLUDE_DIRS
        .
        ui
    REQUIRES
        esp_wifi
        esp_event
        esp_netif
        esp_http_client
        json
        mbedtls
        nvs_flash
    PRIV_REQUIRES
        bsp_extra
        esp_driver_gpio
        waveshare__esp32_p4_wifi6_touch_lcd_4b
)

target_compile_options(
    ${COMPONENT_LIB}
    PRIVATE
        -DLV_LVGL_H_INCLUDE_SIMPLE
)

idf_component_get_property(LVGL_LIB lvgl__lvgl COMPONENT_LIB)
target_compile_options(
    ${LVGL_LIB} 
    PRIVATE 
        -DLV_LVGL_H_INCLUDE_SIMPLE
        "-DLV_MEM_CUSTOM_ALLOC(x)=heap_caps_malloc(x, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)"
        "-DLV_MEM_CUSTOM_FREE=heap_caps_free"
)
