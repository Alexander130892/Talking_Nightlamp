/**
 * @file  app.c
 * @brief Application: boot sequence and task wiring.
 *
 * Skeleton: each driver init is called and its result logged, so the boot
 * log shows which blocks still have to be implemented.
 */

#include "esp_log.h"
#include "app.h"
#include "board.h"
#include "audio.h"
#include "storage.h"
#include "rc522.h"
#include "lamp_strip.h"
#include "input.h"

static const char *TAG = "app";

static void log_init(const char *p_name, esp_err_t err)
{
    if (ESP_OK == err)
    {
        ESP_LOGI(TAG, "%-10s ok", p_name);
    }
    else
    {
        ESP_LOGW(TAG, "%-10s %s", p_name, esp_err_to_name(err));
    }
}

void app_start(void)
{
    /* 1. Amp muted first: no pop while everything else powers up. */
    log_init("audio", audio_init());

    /* 2. Storage, 3. RFID, 4. lamp, then controls. */
    log_init("storage", storage_init());
    log_init("rc522", rc522_init());
    log_init("lamp", lamp_strip_init(BOARD_LED_COUNT));
    log_init("input", input_init());

    /* TODO: create the audio, rfid, input, app and lamp tasks
     *       (docs/firmware/architecture.md, "Tasks"). */
    ESP_LOGI(TAG, "skeleton boot complete");
}
