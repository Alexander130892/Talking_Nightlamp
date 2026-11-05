/**
 * @file  app.c
 * @brief Application: boot sequence and task wiring.
 *
 * Boot: drivers in the order of docs/firmware/architecture.md, then the app
 * task. The app task is glue only: it feeds events into app_logic (the
 * host-tested behaviour) and executes the commands that come back.
 *
 * Waiting on three sources: block on the RFID poll (one every 150 ms) with
 * a short timeout, then drain input and audio events without waiting.
 * Worst-case reaction to a button is APP_TICK_MS.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "app_logic.h"
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
#define PRIO_APP   (4u)
#define PRIO_INPUT (5u)
#define PRIO_LAMP  (3u)
#define PRIO_RFID  (6u)
#define PRIO_AUDIO (10u)   /* highest: never starve the I2S DMA */

/* Volume (ADR 0009). Max gain is set after test C6 with the final GAIN pin. */
#define AUDIO_MAX_GAIN_DB   (0)
#define AUDIO_START_VOLUME  (APP_VOLUME_START)

#define APP_TICK_MS         (20u)
#define APP_STACK           (4096u)

_Static_assert(APP_VOLUME_MAX == (AUDIO_VOLUME_STEPS - 1u), "volume ranges differ");

static app_logic_t s_app;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static const char *cmd_name(app_cmd_type_t t)
{
    static const char *names[] =
    {
        "PLAY", "PAUSE", "RESUME", "STOP", "CHIME", "VOLUME", "MUTE",
        "LAMP_FADE", "LAMP_PULSE", "LAMP_BREATHE", "LAMP_TINT",
    };
    return ((size_t)t < (sizeof(names) / sizeof(names[0]))) ? names[t] : "?";
}

/** Execute the commands app_logic returned. */
static void run(const app_cmds_t *p_c)
{
    for (uint8_t i = 0u; i < p_c->n; i++)
    {
        const app_cmd_t *p = &p_c->cmd[i];

        switch (p->type)
        {
            case APP_CMD_AUDIO_PLAY:    audio_play_story(p->arg);              break;
            case APP_CMD_AUDIO_PAUSE:   audio_pause();                         break;
            case APP_CMD_AUDIO_RESUME:  audio_resume();                        break;
            case APP_CMD_AUDIO_STOP:    audio_stop();                          break;
            case APP_CMD_AUDIO_CHIME:   audio_play_chime();                    break;
            case APP_CMD_AUDIO_VOLUME:  audio_set_volume(p->arg);              break;
            case APP_CMD_AUDIO_MUTE:    audio_set_mute(0u != p->arg);          break;
            case APP_CMD_LAMP_FADE:     lamp_fade_to(p->level_pm, p->dur_ms);  break;
            case APP_CMD_LAMP_PULSE:    lamp_pulse();                          break;
            case APP_CMD_LAMP_BREATHE:  lamp_breathe(0u != p->arg);            break;
            case APP_CMD_LAMP_TINT:     lamp_set_tint(p->arg, p->level_pm);    break;
            default:                                                           break;
        }
        ESP_LOGI(TAG, "  cmd %s arg %u level %u dur %lu", cmd_name(p->type),
                 (unsigned)p->arg, (unsigned)p->level_pm, (unsigned long)p->dur_ms);
    }
}

static void app_task(void *p_arg)
{
    (void)p_arg;

    for (;;)
    {
        card_poll_t   poll;
        input_event_t ievt;
        audio_event_t aevt;
        app_cmds_t    c;

        if (rfid_get_poll(&poll, APP_TICK_MS))
        {
            c = app_logic_poll(&s_app, &poll, now_ms());
            run(&c);
        }
        while (input_get_event(&ievt, 0u))
        {
            const app_input_t in =
                (INPUT_EVT_LAMP_PRESS == ievt.type) ? APP_IN_LAMP_PRESS :
                (INPUT_EVT_ENC_PRESS == ievt.type)  ? APP_IN_ENC_PRESS  :
                                                      APP_IN_ENC_STEP;

            ESP_LOGI(TAG, "input: %s %+d", (APP_IN_LAMP_PRESS == in) ? "lamp button" :
                                           (APP_IN_ENC_PRESS == in)  ? "encoder press" :
                                                                       "encoder step",
                     (int)ievt.steps);
            c = app_logic_input(&s_app, in, ievt.steps, now_ms());
            run(&c);
        }
        while (audio_get_event(&aevt, 0u))
        {
            ESP_LOGI(TAG, "story %03u %s", (unsigned)aevt.story_id,
                     (AUDIO_EVT_STORY_DONE == aevt.type) ? "done" : "could not play");
            c = app_logic_story_end(&s_app, AUDIO_EVT_STORY_ERROR == aevt.type, now_ms());
            run(&c);
        }
        c = app_logic_tick(&s_app, now_ms());
        run(&c);
    }
}

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
    const esp_err_t sd_err = storage_init(&storage_cfg);
    app_cmds_t      c;

    log_init("storage", sd_err);
    log_init("rfid", rfid_start(&rfid_cfg));
    log_init("lamp", lamp_start(&lamp_cfg));
    log_init("input", input_init(&input_cfg));

    /* 5. Behaviour: SD fault means no audio, the lamp still answers */
    c = app_logic_init(&s_app, ESP_OK == sd_err, now_ms());
    run(&c);
    if (pdPASS != xTaskCreate(app_task, "app", APP_STACK, NULL, PRIO_APP, NULL))
    {
        ESP_LOGE(TAG, "app task not started");
        return;
    }
    ESP_LOGI(TAG, "ready%s", (ESP_OK == sd_err) ? "" : " (NO SD CARD: lamp only)");
}
