# ESP32-P4 SDIO and touch stability fixes

## Scope

This project uses an ESP32-P4 host with an ESP32-C6 Wi-Fi coprocessor over
four-bit SDIO. The fixes in this change address two independent reboot paths.

## ESP-Hosted SDIO

ESP32-P4 SDMMC buffers located behind cache must have cache-line-aligned
addresses and transfer sizes. The P4 build uses a 128-byte L2 cache line, while
ESP-Hosted 2.12.8 creates transport pools with 64-byte alignment and does not
round each pool block stride to the requested alignment.

This can make a valid Wi-Fi packet fail with `ESP_ERR_INVALID_ARG` (`258`).
ESP-Hosted treats the repeated transfer failure as unrecoverable and restarts
the host. Moving all pools to internal RAM avoids that alignment path but can
exhaust contiguous internal DMA memory during startup.

The project therefore keeps the pools in DMA-capable PSRAM and applies two
managed-component adjustments during CMake configuration:

- use the P4 cache-line size for DMA buffer alignment;
- round every memory-pool block stride to that alignment.

The patch is applied by `cmake/patch_managed_components.cmake` after IDF
Component Manager resolves the downloaded dependencies.

## GT911 touch controller

The GT911 bus is configured for 100 kHz. Initialization waits 100 ms and is
retried up to three times. A failed attempt releases its panel IO handle. If
all attempts fail, the display remains running without touch instead of
aborting and entering a reboot loop.

## Verification after flashing

Confirm that the serial log does not contain:

- `mempool create failed: no mem`;
- `Failed to send data: 258`;
- `Unrecoverable host sdio state`;
- an `ESP_ERROR_CHECK` abort from `bsp_display_indev_init`.

The ESP32-C6 coprocessor firmware should also be updated separately if the log
continues to report an ESP-Hosted host/coprocessor version mismatch.
