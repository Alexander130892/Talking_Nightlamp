/**
 * @file  08_tone.c
 * @brief Bring-up step 8: I2S -> MAX98357A, test tones (no SD card).
 *
 * Boot order against the power-on pop:
 *   1. amp SD pin low (shutdown)
 *   2. start I2S, stream silence
 *   3. release shutdown
 * Then repeats: 440 Hz 1 s, pause, 1 kHz 1 s, pause, 3 s silence.
 *
 * Level is kept low (TONE_DBFS): this is a first power-up on laptop USB.
 */

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_log.h"

#define PIN_I2S_BCLK     (26)
#define PIN_I2S_LRC      (25)
#define PIN_I2S_DIN      (33)
#define PIN_AMP_SD       (21)   /* low = shutdown, high = on (left ch) */

#define SAMPLE_RATE      (44100u)
#define TONE_DBFS        (-20.0f)   /* -20 dBFS: clearly audible, not loud */
#define RAMP_MS          (10u)      /* fade in/out: no clicks */
#define FRAMES_PER_BLOCK (256u)

static const char *TAG = "tone";
static i2s_chan_handle_t s_tx;
static int16_t s_block[FRAMES_PER_BLOCK * 2u];  /* interleaved L, R */

static void amp_enable(bool b_on)
{
    ESP_ERROR_CHECK(gpio_set_level(PIN_AMP_SD, b_on ? 1 : 0));
}

static void i2s_init(void)
{
    const i2s_chan_config_t chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    const i2s_std_config_t std_cfg =
    {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                        I2S_SLOT_MODE_STEREO),
        .gpio_cfg =
        {
            .mclk = I2S_GPIO_UNUSED,     /* MAX98357A needs no MCLK */
            .bclk = PIN_I2S_BCLK,
            .ws   = PIN_I2S_LRC,
            .dout = PIN_I2S_DIN,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = { false, false, false },
        },
    };

    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &s_tx, NULL));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_tx, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(s_tx));
}

static void write_block(void)
{
    size_t written = 0u;

    ESP_ERROR_CHECK(i2s_channel_write(s_tx, s_block, sizeof(s_block),
                                      &written, portMAX_DELAY));
}

static void play_silence(uint32_t ms)
{
    const uint32_t frames = (SAMPLE_RATE * ms) / 1000u;

    for (uint32_t i = 0u; i < FRAMES_PER_BLOCK * 2u; i++)
    {
        s_block[i] = 0;
    }
    for (uint32_t done = 0u; done < frames; done += FRAMES_PER_BLOCK)
    {
        write_block();
    }
}

/** Sine tone with linear fade in/out, same sample on both channels. */
static void play_tone(float freq_hz, uint32_t ms)
{
    const float    amp    = 32767.0f * powf(10.0f, TONE_DBFS / 20.0f);
    const uint32_t frames = (SAMPLE_RATE * ms) / 1000u;
    const uint32_t ramp   = (SAMPLE_RATE * RAMP_MS) / 1000u;
    const float    step   = 2.0f * (float)M_PI * freq_hz / (float)SAMPLE_RATE;
    float          phase  = 0.0f;
    uint32_t       n      = 0u;

    ESP_LOGI(TAG, "%4.0f Hz, %lu ms", freq_hz, (unsigned long)ms);

    while (n < frames)
    {
        for (uint32_t i = 0u; i < FRAMES_PER_BLOCK; i++, n++)
        {
            float env = 1.0f;
            float s   = 0.0f;

            if (n < ramp)
            {
                env = (float)n / (float)ramp;
            }
            else if ((frames - n) < ramp)
            {
                env = (float)(frames - n) / (float)ramp;
            }
            if (n < frames)
            {
                s = amp * env * sinf(phase);
            }
            phase += step;
            if (phase > (2.0f * (float)M_PI))
            {
                phase -= 2.0f * (float)M_PI;
            }
            s_block[2u * i]      = (int16_t)s;   /* left  */
            s_block[2u * i + 1u] = (int16_t)s;   /* right */
        }
        write_block();
    }
}

void app_main(void)
{
    const gpio_config_t sd_cfg =
    {
        .pin_bit_mask = (1ULL << PIN_AMP_SD),
        .mode         = GPIO_MODE_OUTPUT,
    };

    /* 1. Amp off before anything else */
    ESP_ERROR_CHECK(gpio_config(&sd_cfg));
    amp_enable(false);

    /* 2. I2S running, silence in the pipeline */
    i2s_init();
    play_silence(200u);

    /* 3. Now wake the amp: it sees a clean zero signal */
    amp_enable(true);
    ESP_LOGI(TAG, "amp enabled, %.0f dBFS tones, BCLK %lu Hz",
             TONE_DBFS, (unsigned long)(SAMPLE_RATE * 2u * 16u));
    play_silence(300u);

    for (;;)
    {
        play_tone(440.0f, 1000u);
        play_silence(500u);
        play_tone(1000.0f, 1000u);
        play_silence(3000u);
    }
}
