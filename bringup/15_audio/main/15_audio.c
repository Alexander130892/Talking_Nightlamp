/**
 * @file  15_audio.c
 * @brief Driver test 15: the audio driver, scripted, volume on the encoder.
 *
 * Needs a LONG story in /stories/007 (a few minutes, ideally 2 parts:
 * 01.mp3 + 02.mp3) to hear pause/resume and part chaining.
 *
 * Script (logged as it goes):
 *   story 099 (missing)      -> STORY_ERROR event
 *   play 007                 -> fade in
 *   +8 s  chime              -> story fades out, chime, story resumes 1.5 s back
 *   +8 s  pause, 3 s, resume -> fade out, silence, fade in 1.5 s back
 *   +8 s  mute 3 s, unmute   -> story keeps running silently
 *   +8 s  play 007 again     -> fade out, restart from the beginning
 *   then plays to the end    -> STORY_DONE event, 5 s, repeat
 * Encoder: volume at any time. Encoder press: chime.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "audio.h"
#include "input.h"
#include "storage.h"

#define STORY (7u)

static const char *TAG = "test";
static int         s_volume = 10;

/** Wait ms while serving the encoder and logging audio events. */
static bool wait(uint32_t ms)
{
    const TickType_t end = xTaskGetTickCount() + pdMS_TO_TICKS(ms);
    bool             b_done = false;

    while ((int32_t)(end - xTaskGetTickCount()) > 0)
    {
        input_event_t ievt;
        audio_event_t aevt;

        if (input_get_event(&ievt, 20u))
        {
            if (INPUT_EVT_ENC_STEP == ievt.type)
            {
                s_volume += ievt.steps;
                s_volume  = (s_volume < 0) ? 0 :
                            ((s_volume > (int)(AUDIO_VOLUME_STEPS - 1u)) ?
                             (int)(AUDIO_VOLUME_STEPS - 1u) : s_volume);
                audio_set_volume((uint8_t)s_volume);
                ESP_LOGI(TAG, "volume %d", s_volume);
            }
            else if (INPUT_EVT_ENC_PRESS == ievt.type)
            {
                ESP_LOGI(TAG, "chime (encoder press)");
                audio_play_chime();
            }
            else
            {
                /* lamp button unused here */
            }
        }
        if (audio_get_event(&aevt, 0u))
        {
            ESP_LOGI(TAG, "EVENT %s story %03u",
                     (AUDIO_EVT_STORY_DONE == aevt.type) ? "STORY_DONE" : "STORY_ERROR",
                     (unsigned)aevt.story_id);
            b_done = true;
        }
    }
    return b_done;
}

void app_main(void)
{
    const audio_config_t audio_cfg =
    {
        .bclk = GPIO_NUM_26, .ws = GPIO_NUM_25, .dout = GPIO_NUM_33,
        .amp_sd = GPIO_NUM_21, .max_gain_db = 0, .start_volume = (uint8_t)s_volume,
        .task_prio = 10u, .core = 1,
    };
    const storage_config_t sd_cfg =
    {
        .host = SPI2_HOST, .sck = GPIO_NUM_14, .miso = GPIO_NUM_27,
        .mosi = GPIO_NUM_13, .cs = GPIO_NUM_4, .freq_khz = 10000,
    };
    const input_config_t in_cfg =
    {
        .lamp_btn = GPIO_NUM_16, .enc_sw = GPIO_NUM_17, .enc_a = GPIO_NUM_34,
        .enc_b = GPIO_NUM_35, .counts_per_step = 4, .task_prio = 5u,
    };

    ESP_ERROR_CHECK(audio_init(&audio_cfg));      /* first: amp silent */
    ESP_ERROR_CHECK(storage_init(&sd_cfg));
    ESP_ERROR_CHECK(input_init(&in_cfg));

    ESP_LOGI(TAG, "story 099 (missing): expect STORY_ERROR");
    audio_play_story(99u);
    (void)wait(1000u);

    for (;;)
    {
        ESP_LOGI(TAG, "play %03u", STORY);
        audio_play_story(STORY);
        (void)wait(8000u);

        ESP_LOGI(TAG, "chime: story fades, chime, resumes 1.5 s back");
        audio_play_chime();
        (void)wait(8000u);

        ESP_LOGI(TAG, "pause 3 s");
        audio_pause();
        (void)wait(3000u);
        ESP_LOGI(TAG, "resume: 1.5 s back");
        audio_resume();
        (void)wait(8000u);

        ESP_LOGI(TAG, "mute 3 s (story keeps running)");
        audio_set_mute(true);
        (void)wait(3000u);
        ESP_LOGI(TAG, "unmute");
        audio_set_mute(false);
        (void)wait(8000u);

        ESP_LOGI(TAG, "play %03u again: restart", STORY);
        audio_play_story(STORY);

        ESP_LOGI(TAG, "playing to the end ...");
        while (!wait(1000u))
        {
        }
        (void)wait(5000u);
    }
}
