# Keep project-specific fixes for downloaded IDF components reproducible.
# Each replacement is idempotent: an already patched tree is left unchanged.

function(_replace_managed_component file_path old_text new_text description)
    if(NOT EXISTS "${file_path}")
        message(FATAL_ERROR "Managed component file not found: ${file_path}")
    endif()

    file(READ "${file_path}" file_content)
    string(FIND "${file_content}" "${new_text}" new_position)
    if(NOT new_position EQUAL -1)
        return()
    endif()

    string(FIND "${file_content}" "${old_text}" old_position)
    if(old_position EQUAL -1)
        message(FATAL_ERROR "Cannot apply managed component fix: ${description}")
    endif()

    string(REPLACE "${old_text}" "${new_text}" file_content "${file_content}")
    file(WRITE "${file_path}" "${file_content}")
    message(STATUS "Applied managed component fix: ${description}")
endfunction()

set(ESP_HOSTED_ROOT "${CMAKE_SOURCE_DIR}/managed_components/espressif__esp_hosted")
set(WAVESHARE_BSP_ROOT "${CMAKE_SOURCE_DIR}/managed_components/waveshare__esp32_p4_wifi6_touch_lcd_4b")

_replace_managed_component(
    "${ESP_HOSTED_ROOT}/host/port/esp/freertos/include/port_esp_hosted_host_os.h"
    [=[#define HOSTED_MEM_ALIGNMENT_64     64]=]
    [=[#if CONFIG_IDF_TARGET_ESP32P4
/* SDMMC buffers behind the P4 cache require cache-line-aligned address and size. */
#define HOSTED_MEM_ALIGNMENT_64     CONFIG_CACHE_L2_CACHE_LINE_SIZE
#else
#define HOSTED_MEM_ALIGNMENT_64     64
#endif]=]
    "use the ESP32-P4 cache-line size for hosted DMA buffers"
)

_replace_managed_component(
    "${ESP_HOSTED_ROOT}/common/mempool/mempool.c"
    [=[	struct hosted_mempool_t *new = NULL;
	struct os_mempool *pool = NULL;
	uint8_t *heap = NULL;]=]
    [=[	struct hosted_mempool_t *new = NULL;
	struct os_mempool *pool = NULL;
	uint8_t *heap = NULL;
	size_t pool_block_size = OS_ALIGN(config->block_size, config->alignment_in_bytes);
	size_t pool_size = OS_MEMPOOL_BYTES(config->num_blocks, pool_block_size);]=]
    "calculate an aligned stride for every hosted memory-pool block"
)

_replace_managed_component(
    "${ESP_HOSTED_ROOT}/common/mempool/mempool.c"
    [=[		heap = (uint8_t *)config->malloc(MEMPOOL_ALIGNED(
				OS_MEMPOOL_BYTES(config->num_blocks, config->block_size),
				config->alignment_in_bytes),
				HOSTED_MEM_CAP_DMA);]=]
    [=[		heap = (uint8_t *)config->malloc(MEMPOOL_ALIGNED(pool_size,
				config->alignment_in_bytes),
				HOSTED_MEM_CAP_DMA);]=]
    "allocate enough backing memory for aligned hosted blocks"
)

_replace_managed_component(
    "${ESP_HOSTED_ROOT}/common/mempool/mempool.c"
    [=[		if (config->pre_allocated_mem_size < OS_MEMPOOL_BYTES(config->num_blocks,
				config->block_size)) {]=]
    [=[		if (config->pre_allocated_mem_size < pool_size) {]=]
    "validate preallocated hosted memory using the aligned pool size"
)

_replace_managed_component(
    "${ESP_HOSTED_ROOT}/common/mempool/mempool.c"
    [=[	if (new->ops->mempool_init(pool, config->num_blocks, config->block_size, heap, str)) {]=]
    [=[	/* Keep every block aligned, not only the first block in the backing heap. */
	if (new->ops->mempool_init(pool, config->num_blocks, pool_block_size, heap, str)) {]=]
    "initialize hosted pools with the aligned block stride"
)

_replace_managed_component(
    "${WAVESHARE_BSP_ROOT}/esp32_p4_wifi6_touch_lcd_4b.c"
    [=[#include "esp_codec_dev_defaults.h"]=]
    [=[#include "esp_codec_dev_defaults.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"]=]
    "include FreeRTOS timing APIs for GT911 startup retries"
)

_replace_managed_component(
    "${WAVESHARE_BSP_ROOT}/esp32_p4_wifi6_touch_lcd_4b.c"
    [=[    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(i2c_handle, &tp_io_config, &tp_io_handle), TAG, "");
    return esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, ret_touch);]=]
    [=[    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(i2c_handle, &tp_io_config, &tp_io_handle), TAG, "");
    esp_err_t ret = esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, ret_touch);
    if (ret != ESP_OK) {
        esp_lcd_panel_io_del(tp_io_handle);
    }
    return ret;]=]
    "release the GT911 panel IO handle after a failed initialization"
)

_replace_managed_component(
    "${WAVESHARE_BSP_ROOT}/esp32_p4_wifi6_touch_lcd_4b.c"
    [=[static lv_indev_t *bsp_display_indev_init(lv_display_t *disp)
{
    esp_lcd_touch_handle_t tp;
    BSP_ERROR_CHECK_RETURN_NULL(bsp_touch_new(NULL, &tp));
    assert(tp);

    /* Add touch input (for selected screen) */]=]
    [=[static lv_indev_t *bsp_display_indev_init(lv_display_t *disp)
{
    const unsigned int max_attempts = 3;
    esp_lcd_touch_handle_t tp = NULL;
    esp_err_t ret = ESP_FAIL;

    /* GT911 shares reset with the LCD and may still be starting after a warm reset. */
    for (unsigned int attempt = 1; attempt <= max_attempts; ++attempt) {
        vTaskDelay(pdMS_TO_TICKS(100));
        ret = bsp_touch_new(NULL, &tp);
        if (ret == ESP_OK && tp != NULL) {
            break;
        }
        ESP_LOGW(TAG, "GT911 initialization attempt %u/%u failed: %s",
                 attempt, max_attempts, esp_err_to_name(ret));
    }

    if (ret != ESP_OK || tp == NULL) {
        ESP_LOGE(TAG, "GT911 unavailable after %u attempts; display will continue without touch",
                 max_attempts);
        return NULL;
    }

    /* Add touch input (for selected screen) */]=]
    "retry GT911 startup and keep the display available on touch failure"
)

_replace_managed_component(
    "${WAVESHARE_BSP_ROOT}/esp32_p4_wifi6_touch_lcd_4b.c"
    [=[    BSP_NULL_CHECK(disp_indev = bsp_display_indev_init(disp), NULL);

    return disp;]=]
    [=[    disp_indev = bsp_display_indev_init(disp);
    if (disp_indev == NULL) {
        ESP_LOGW(TAG, "Display started without a touch input device");
    }

    return disp;]=]
    "allow the display to start when the touch controller is unavailable"
)
