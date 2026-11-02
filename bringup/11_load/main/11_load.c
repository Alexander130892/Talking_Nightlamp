/**
 * @file  11_load.c
 * @brief Bring-up step 11c: worst-case load on the lamp's own USB-C input.
 *
 * Four phases of PHASE_MS each, repeated. Measure the star point and the
 * ESP32 VIN pin with a multimeter in every phase:
 *   A idle         : ESP32 only
 *   B LEDs         : all 12 LEDs "full RGBW" requested, capped by power_limit
 *   C LEDs + audio : B plus a loud 200 Hz tone  <- worst case
 *   D audio        : tone only
 *
 * A multimeter shows the average; audio peaks are milliseconds. A brownout
 * would show up as a reset: the reset reason is logged at every boot.
 *
 * The tone is LOUD on purpose (TONE_DBFS): this is the worst case the supply
 * must survive. Keep the speaker away from your ears.
 */

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "driver/rmt_tx.h"
#include "esp_system.h"
#include "esp_log.h"
#include "pixel.h"
#include "power_limit.h"

#define PIN_I2S_BCLK     (26)
#define PIN_I2S_LRC      (25)
#define PIN_I2S_DIN      (33)
#define PIN_AMP_SD       (21)
#define PIN_LED_DATA     (32)

#define LED_COUNT        (12u)
#define SAMPLE_RATE      (44100u)
#define TONE_HZ          (200.0f)
#define TONE_DBFS        (-3.0f)
#define FRAMES_PER_BLOCK (256u)
#define PHASE_MS         (5000u)

static const char *TAG = "load";

static i2s_chan_handle_t    s_tx;
static rmt_channel_handle_t s_led;
static rmt_encoder_handle_t s_enc;
static pixel_grbw_t         s_px[LED_COUNT];
static int16_t              s_block[FRAMES_PER_BLOCK * 2u];
static float                s_phase;

static const char *reset_name(esp_reset_reason_t r)
{
    switch (r)
    {
        case ESP_RST_POWERON:  return "power-on";
        case ESP_RST_EXT:      return "EN pin";
        case ESP_RST_SW:       return "software";
        case ESP_RST_PANIC:    return "panic";
        case ESP_RST_BROWNOUT: return "BROWNOUT";
        default:               return "other";
    }
}

static void led_init(void)
{
    const rmt_tx_channel_config_t chan_cfg =
    {
        .gpio_num          = PIN_LED_DATA,
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .resolution_hz     = 10000000u,
        .mem_block_symbols = 128u,
        .trans_queue_depth = 4,
    };
    const rmt_bytes_encoder_config_t enc_cfg =
    {
        .bit0  = { .level0 = 1, .duration0 = 3, .level1 = 0, .duration1 = 9 },
        .bit1  = { .level0 = 1, .duration0 = 6, .level1 = 0, .duration1 = 6 },
        .flags = { .msb_first = 1 },
    };

    ESP_ERROR_CHECK(rmt_new_tx_channel(&chan_cfg, &s_led));
    ESP_ERROR_CHECK(rmt_new_bytes_encoder(&enc_cfg, &s_enc));
    ESP_ERROR_CHECK(rmt_enable(s_led));
}

/** Request every channel at 255 (or 0), cap with the real limiter, send. */
static void led_full(bool b_on)
{
    const rmt_transmit_config_t tx_cfg = { .loop_count = 0 };
    const uint8_t               v      = b_on ? 255u : 0u;
    uint32_t                    before;

    for (uint32_t i = 0u; i < LED_COUNT; i++)
    {
        s_px[i] = (pixel_grbw_t){ .g = v, .r = v, .b = v, .w = v };
    }
    before = power_limit_estimate_ma(s_px, LED_COUNT);
    power_limit_apply(s_px, LED_COUNT, POWER_LIMIT_BUDGET_MA);
    if (b_on)
    {
        ESP_LOGI(TAG, "  LEDs requested %lu mA -> capped %lu mA (each ch %u/255)",
                 (unsigned long)before,
                 (unsigned long)power_limit_estimate_ma(s_px, LED_COUNT),
                 (unsigned)s_px[0].w);
    }
    ESP_ERROR_CHECK(rmt_transmit(s_led, s_enc, s_px, sizeof(s_px), &tx_cfg));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(s_led, portMAX_DELAY));
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
            .mclk = I2S_GPIO_UNUSED,
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

/** Stream PHASE_MS of tone (b_tone) or silence; I2S paces the loop. */
static void audio_phase(bool b_tone)
{
    const float    amp    = 32767.0f * powf(10.0f, TONE_DBFS / 20.0f);
    const float    step   = 2.0f * (float)M_PI * TONE_HZ / (float)SAMPLE_RATE;
    const uint32_t frames = (SAMPLE_RATE * PHASE_MS) / 1000u;

    for (uint32_t done = 0u; done < frames; done += FRAMES_PER_BLOCK)
    {
        size_t written;

        for (uint32_t i = 0u; i < FRAMES_PER_BLOCK; i++)
        {
            const int16_t s = b_tone ? (int16_t)(amp * sinf(s_phase)) : 0;

            s_phase += step;
            if (s_phase > (2.0f * (float)M_PI))
            {
                s_phase -= 2.0f * (float)M_PI;
            }
            s_block[2u * i]      = s;
            s_block[2u * i + 1u] = s;
        }
        ESP_ERROR_CHECK(i2s_channel_write(s_tx, s_block, sizeof(s_block),
                                          &written, portMAX_DELAY));
    }
}

static void phase(const char *p_name, bool b_leds, bool b_tone)
{
    ESP_LOGI(TAG, "%s  -> measure star point and VIN now", p_name);
    led_full(b_leds);
    audio_phase(b_tone);
}

void app_main(void)
{
    const gpio_config_t sd_cfg =
    {
        .pin_bit_mask = (1ULL << PIN_AMP_SD),
        .mode         = GPIO_MODE_OUTPUT,
    };

    ESP_LOGI(TAG, "reset reason: %s", reset_name(esp_reset_reason()));

    ESP_ERROR_CHECK(gpio_config(&sd_cfg));
    ESP_ERROR_CHECK(gpio_set_level(PIN_AMP_SD, 0));   /* amp off */
    led_init();
    led_full(false);                                   /* strip dark */
    i2s_init();
    audio_phase(false);                                /* silence in DMA */
    ESP_ERROR_CHECK(gpio_set_level(PIN_AMP_SD, 1));   /* amp on */

    for (uint32_t round = 1u; ; round++)
    {
        ESP_LOGI(TAG, "round %lu, %u s per phase, tone %.0f Hz %.0f dBFS",
                 (unsigned long)round, (unsigned)(PHASE_MS / 1000u),
                 TONE_HZ, TONE_DBFS);
        phase("A idle        ", false, false);
        phase("B LEDs        ", true,  false);
        phase("C LEDs + audio", true,  true);
        phase("D audio       ", false, true);
    }
}
