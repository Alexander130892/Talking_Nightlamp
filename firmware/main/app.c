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
#include "rfid.h"
#include "lamp.h"
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

/* Task priorities (docs/firmware/architecture.md, "Tasks") */
#define PRIO_INPUT (5u)
#define PRIO_LAMP  (3u)
#define PRIO_RFID  (6u)
#define PRIO_AUDIO (10u)   /* highest: never starve the I2S DMA */

/* Volume (ADR 0009). Max gain is set after test C6 with the final GAIN pin. */
#define AUDIO_MAX_GAIN_DB   (0)
#define AUDIO_START_VOLUME  (10u)

void app_start(void)
{
    const input_config_t input_cfg =
    {
        .lamp_btn        = BOARD_BTN_LAMP,
        .enc_sw          = BOARD_ENC_SW,
        .enc_a           = BOARD_ENC_A,
        .enc_b           = BOARD_ENC_B,
        .counts_per_step = BOARD_ENC_COUNTS_PER_STEP,
        .task_prio       = PRIO_INPUT,
    };
    const audio_config_t audio_cfg =
    {
        .bclk         = BOARD_I2S_BCLK,
        .ws           = BOARD_I2S_LRC,
        .dout         = BOARD_I2S_DIN,
        .amp_sd       = BOARD_AMP_SD,
        .max_gain_db  = AUDIO_MAX_GAIN_DB,
        .start_volume = AUDIO_START_VOLUME,
        .task_prio    = PRIO_AUDIO,
        .core         = 1,
    };
    const storage_config_t storage_cfg =
    {
        .host     = SPI2_HOST,
        .sck      = BOARD_SD_SCK,
        .miso     = BOARD_SD_MISO,
        .mosi     = BOARD_SD_MOSI,
        .cs       = BOARD_SD_CS,
        .freq_khz = BOARD_SD_FREQ_KHZ,
    };
    const rfid_config_t rfid_cfg =
    {
        .rc522 =
        {
            .host     = SPI3_HOST,
            .sck      = BOARD_RC522_SCK,
            .miso     = BOARD_RC522_MISO,
            .mosi     = BOARD_RC522_MOSI,
            .cs       = BOARD_RC522_CS,
            .rst      = BOARD_RC522_RST,
            .clock_hz = BOARD_RC522_CLOCK_HZ,
        },
        .task_prio = PRIO_RFID,
    };
    const lamp_config_t lamp_cfg =
    {
        .strip     = { .data_gpio = BOARD_LED_DATA, .n_pixels = BOARD_LED_COUNT },
        .task_prio = PRIO_LAMP,
    };

    /* 1. Amp muted first: no pop while everything else powers up. */
    log_init("audio", audio_init(&audio_cfg));

    /* 2. Storage, 3. RFID, 4. lamp, then controls. */
    log_init("storage", storage_init(&storage_cfg));
    log_init("rfid", rfid_start(&rfid_cfg));
    log_init("lamp", lamp_start(&lamp_cfg));
    log_init("input", input_init(&input_cfg));

    /* TODO: create the audio, rfid, input, app and lamp tasks
     *       (docs/firmware/architecture.md, "Tasks"). */
    ESP_LOGI(TAG, "skeleton boot complete");
}
