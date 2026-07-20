#include "app_microphone.h"

#include <inttypes.h>
#include <string.h>

#include "bsp/esp-bsp.h"
#include "esp_codec_dev.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define APP_MICROPHONE_BUFFER_BYTES 2048U
#define APP_MICROPHONE_TASK_STACK_SIZE 4096U
#define APP_MICROPHONE_TASK_PRIORITY 5U
#define APP_MICROPHONE_LOG_INTERVAL_MS 500U
#define APP_MICROPHONE_INPUT_GAIN_DB 24.0f

static const char *TAG = "app_microphone";

static esp_codec_dev_handle_t s_record_dev;
static TaskHandle_t s_capture_task;
static bool s_initialized;
static bool s_capturing;
static bool s_stop_requested;
static app_microphone_stats_t s_stats;
static app_microphone_pcm_callback_t s_pcm_callback;
static void *s_pcm_callback_ctx;
static portMUX_TYPE s_state_lock = portMUX_INITIALIZER_UNLOCKED;

static uint16_t sample_abs(int16_t sample)
{
    int32_t value = sample;
    if (value < 0) {
        value = -value;
    }
    return (uint16_t)value;
}

static void set_capture_error(esp_err_t error)
{
    taskENTER_CRITICAL(&s_state_lock);
    s_stats.last_error = error;
    taskEXIT_CRITICAL(&s_state_lock);
}

static void finish_capture_task(void)
{
    taskENTER_CRITICAL(&s_state_lock);
    s_capturing = false;
    s_stop_requested = false;
    s_capture_task = NULL;
    taskEXIT_CRITICAL(&s_state_lock);
}

static bool capture_should_continue(void)
{
    taskENTER_CRITICAL(&s_state_lock);
    bool should_continue = s_capturing && !s_stop_requested;
    taskEXIT_CRITICAL(&s_state_lock);
    return should_continue;
}

static void microphone_capture_task(void *arg)
{
    (void)arg;

    int16_t *samples = heap_caps_malloc(APP_MICROPHONE_BUFFER_BYTES,
                                        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (samples == NULL) {
        ESP_LOGE(TAG, "Failed to allocate %u-byte PCM buffer", APP_MICROPHONE_BUFFER_BYTES);
        set_capture_error(ESP_ERR_NO_MEM);
        finish_capture_task();
        vTaskDelete(NULL);
        return;
    }

    TickType_t last_log_tick = xTaskGetTickCount();
    ESP_LOGI(TAG, "Capture started: %" PRIu32 " Hz, %u-bit, %u channels",
             APP_MICROPHONE_SAMPLE_RATE_HZ,
             APP_MICROPHONE_BITS_PER_SAMPLE,
             APP_MICROPHONE_CHANNEL_COUNT);

    while (capture_should_continue()) {
        int codec_ret = esp_codec_dev_read(s_record_dev, samples, APP_MICROPHONE_BUFFER_BYTES);
        if (codec_ret != ESP_CODEC_DEV_OK) {
            ESP_LOGE(TAG, "PCM read failed: codec error %d", codec_ret);
            set_capture_error(ESP_FAIL);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        const size_t bytes_read = APP_MICROPHONE_BUFFER_BYTES;
        const size_t channel_sample_count = bytes_read / sizeof(samples[0]);
        uint32_t peak = 0;
        uint64_t absolute_sum = 0;

        for (size_t i = 0; i < channel_sample_count; ++i) {
            uint16_t magnitude = sample_abs(samples[i]);
            absolute_sum += magnitude;
            if (magnitude > peak) {
                peak = magnitude;
            }
        }

        uint16_t mean_abs = channel_sample_count > 0
                                ? (uint16_t)(absolute_sum / channel_sample_count)
                                : 0;
        app_microphone_pcm_callback_t callback;
        void *callback_ctx;
        uint64_t total_bytes;
        uint32_t read_count;

        taskENTER_CRITICAL(&s_state_lock);
        s_stats.total_bytes += bytes_read;
        s_stats.total_channel_samples += channel_sample_count;
        s_stats.read_count++;
        s_stats.last_peak = (uint16_t)peak;
        s_stats.last_mean_abs = mean_abs;
        s_stats.last_error = ESP_OK;
        total_bytes = s_stats.total_bytes;
        read_count = s_stats.read_count;
        callback = s_pcm_callback;
        callback_ctx = s_pcm_callback_ctx;
        taskEXIT_CRITICAL(&s_state_lock);

        if (callback != NULL) {
            callback(samples, channel_sample_count, callback_ctx);
        }

        TickType_t now = xTaskGetTickCount();
        if (callback == NULL &&
            (now - last_log_tick) >= pdMS_TO_TICKS(APP_MICROPHONE_LOG_INTERVAL_MS)) {
            uint32_t level_percent = peak * 100U / INT16_MAX;
            ESP_LOGI(TAG,
                     "PCM reads=%" PRIu32 " total=%" PRIu64
                     " bytes peak=%" PRIu32 " mean_abs=%u level=%" PRIu32
                     "%% samples=[%d,%d,%d,%d,%d,%d,%d,%d]",
                     read_count,
                     total_bytes,
                     peak,
                     mean_abs,
                     level_percent,
                     samples[0], samples[1], samples[2], samples[3],
                     samples[4], samples[5], samples[6], samples[7]);
            last_log_tick = now;
        }
    }

    app_microphone_stats_t final_stats = {0};
    app_microphone_get_stats(&final_stats);
    ESP_LOGI(TAG, "Capture stopped: reads=%" PRIu32 ", total=%" PRIu64 " bytes",
             final_stats.read_count, final_stats.total_bytes);

    heap_caps_free(samples);
    finish_capture_task();
    vTaskDelete(NULL);
}

esp_err_t app_microphone_init(void)
{
    if (app_microphone_is_initialized()) {
        return ESP_OK;
    }

    esp_codec_dev_handle_t record_dev = bsp_audio_codec_microphone_init();
    if (record_dev == NULL) {
        ESP_LOGE(TAG, "Board microphone codec initialization failed");
        return ESP_FAIL;
    }

    esp_codec_dev_sample_info_t sample_info = {
        .bits_per_sample = APP_MICROPHONE_BITS_PER_SAMPLE,
        .channel = APP_MICROPHONE_CHANNEL_COUNT,
        .channel_mask = 0,
        .sample_rate = APP_MICROPHONE_SAMPLE_RATE_HZ,
        .mclk_multiple = 0,
    };

    int codec_ret = esp_codec_dev_open(record_dev, &sample_info);
    if (codec_ret != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "Failed to open microphone codec: %d", codec_ret);
        return ESP_FAIL;
    }

    codec_ret = esp_codec_dev_set_in_gain(record_dev, APP_MICROPHONE_INPUT_GAIN_DB);
    if (codec_ret != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "Failed to set microphone input gain: %d", codec_ret);
        return ESP_FAIL;
    }

    taskENTER_CRITICAL(&s_state_lock);
    s_record_dev = record_dev;
    s_initialized = true;
    memset(&s_stats, 0, sizeof(s_stats));
    taskEXIT_CRITICAL(&s_state_lock);

    ESP_LOGI(TAG, "Board microphone ready: ES7210, %" PRIu32 " Hz, %u-bit, %u channels, gain %.1f dB",
             APP_MICROPHONE_SAMPLE_RATE_HZ,
             APP_MICROPHONE_BITS_PER_SAMPLE,
             APP_MICROPHONE_CHANNEL_COUNT,
             APP_MICROPHONE_INPUT_GAIN_DB);
    return ESP_OK;
}

esp_err_t app_microphone_start(void)
{
    if (!app_microphone_is_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }

    taskENTER_CRITICAL(&s_state_lock);
    if (s_capturing || s_capture_task != NULL) {
        taskEXIT_CRITICAL(&s_state_lock);
        return ESP_OK;
    }
    memset(&s_stats, 0, sizeof(s_stats));
    s_capturing = true;
    s_stop_requested = false;
    taskEXIT_CRITICAL(&s_state_lock);

    BaseType_t created = xTaskCreate(microphone_capture_task,
                                     "app_microphone",
                                     APP_MICROPHONE_TASK_STACK_SIZE,
                                     NULL,
                                     APP_MICROPHONE_TASK_PRIORITY,
                                     &s_capture_task);
    if (created != pdPASS) {
        taskENTER_CRITICAL(&s_state_lock);
        s_capturing = false;
        s_stop_requested = false;
        s_capture_task = NULL;
        s_stats.last_error = ESP_ERR_NO_MEM;
        taskEXIT_CRITICAL(&s_state_lock);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t app_microphone_stop(void)
{
    taskENTER_CRITICAL(&s_state_lock);
    if (s_capturing) {
        s_stop_requested = true;
    }
    taskEXIT_CRITICAL(&s_state_lock);
    return ESP_OK;
}

bool app_microphone_is_initialized(void)
{
    taskENTER_CRITICAL(&s_state_lock);
    bool initialized = s_initialized;
    taskEXIT_CRITICAL(&s_state_lock);
    return initialized;
}

bool app_microphone_is_capturing(void)
{
    taskENTER_CRITICAL(&s_state_lock);
    bool capturing = s_capturing;
    taskEXIT_CRITICAL(&s_state_lock);
    return capturing;
}

esp_err_t app_microphone_get_stats(app_microphone_stats_t *stats)
{
    if (stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    taskENTER_CRITICAL(&s_state_lock);
    *stats = s_stats;
    taskEXIT_CRITICAL(&s_state_lock);
    return ESP_OK;
}

esp_err_t app_microphone_set_pcm_callback(app_microphone_pcm_callback_t callback, void *user_ctx)
{
    taskENTER_CRITICAL(&s_state_lock);
    s_pcm_callback = callback;
    s_pcm_callback_ctx = user_ctx;
    taskEXIT_CRITICAL(&s_state_lock);
    return ESP_OK;
}
