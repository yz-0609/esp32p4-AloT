#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_MICROPHONE_SAMPLE_RATE_HZ 16000U
#define APP_MICROPHONE_BITS_PER_SAMPLE 16U
#define APP_MICROPHONE_CHANNEL_COUNT 2U

typedef struct {
    uint64_t total_bytes;
    uint64_t total_channel_samples;
    uint32_t read_count;
    uint16_t last_peak;
    uint16_t last_mean_abs;
    esp_err_t last_error;
} app_microphone_stats_t;

/**
 * @brief Receives interleaved signed 16-bit PCM from the board microphones.
 *
 * The callback runs in the microphone capture task. It must not block or call
 * LVGL directly. A later VAD/ASR pipeline can register here without changing
 * the microphone driver.
 */
typedef void (*app_microphone_pcm_callback_t)(const int16_t *samples,
                                               size_t channel_sample_count,
                                               void *user_ctx);

/** Initialize the board ES7210 microphone path and open it for PCM input. */
esp_err_t app_microphone_init(void);

/** Start the background PCM capture task. */
esp_err_t app_microphone_start(void);

/** Request the background PCM capture task to stop. This call is non-blocking. */
esp_err_t app_microphone_stop(void);

/** Return true after the microphone hardware has been initialized successfully. */
bool app_microphone_is_initialized(void);

/** Return true while the background capture task is reading PCM. */
bool app_microphone_is_capturing(void);

/** Copy the latest capture statistics. */
esp_err_t app_microphone_get_stats(app_microphone_stats_t *stats);

/** Register an optional consumer for future VAD, recording, or ASR processing. */
esp_err_t app_microphone_set_pcm_callback(app_microphone_pcm_callback_t callback, void *user_ctx);

#ifdef __cplusplus
}
#endif
