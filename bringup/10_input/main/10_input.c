/**
 * @file  10_input.c
 * @brief Bring-up step 10: lamp button, encoder (volume) and encoder push (mute).
 *
 * Encoder A/B -> PCNT peripheral in quadrature (x4) mode:
 *   every edge on A or B is counted, the level of the other line gives the
 *   direction. Same Gray-code logic as a software state table, but in
 *   hardware: bounce that goes +1 then -1 cancels without CPU involvement.
 *   PCNT glitch filter drops pulses < 10 us on top of the 10k/10n RC filter.
 *
 * Buttons -> polled every 10 ms, accepted after 3 equal samples (30 ms).
 *
 * Logs: lamp toggle (on-board LED GPIO 2 mirrors it), volume steps with raw
 * count, mute toggle. No audio or strip involved.
 */

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "esp_log.h"

#define PIN_LAMP_BTN     (16)   /* NO -> GND, internal pull-up */
#define PIN_ENC_SW       (17)   /* -> GND, internal pull-up */
#define PIN_ENC_A        (34)   /* external 10k pull-up + 10 nF */
#define PIN_ENC_B        (35)
#define PIN_BOARD_LED    (2)

#define POLL_MS          (10u)
#define DEBOUNCE_SAMPLES (3u)
#define COUNTS_PER_STEP  (4)    /* EC11 20 det / 20 pulse: 4 edges per click */
#define PCNT_LIMIT       (1000)
#define GLITCH_NS        (10000u)
#define VOL_MAX          (20)   /* firmware ceiling, ADR 0009 */

static const char *TAG = "input";

typedef struct
{
    gpio_num_t pin;
    bool       b_pressed;   /* debounced state */
    uint32_t   same;        /* consecutive samples that disagree with it */
} button_t;

static pcnt_unit_handle_t encoder_init(void)
{
    const pcnt_unit_config_t unit_cfg =
    {
        .low_limit  = -PCNT_LIMIT,
        .high_limit = PCNT_LIMIT,
        .flags      = { .accum_count = 1 },  /* keep counting past limits */
    };
    const pcnt_glitch_filter_config_t filt_cfg = { .max_glitch_ns = GLITCH_NS };
    const pcnt_chan_config_t a_cfg =
    {
        .edge_gpio_num  = PIN_ENC_A,
        .level_gpio_num = PIN_ENC_B,
    };
    const pcnt_chan_config_t b_cfg =
    {
        .edge_gpio_num  = PIN_ENC_B,
        .level_gpio_num = PIN_ENC_A,
    };
    pcnt_unit_handle_t    unit;
    pcnt_channel_handle_t ch_a;
    pcnt_channel_handle_t ch_b;

    ESP_ERROR_CHECK(pcnt_new_unit(&unit_cfg, &unit));
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(unit, &filt_cfg));
    ESP_ERROR_CHECK(pcnt_new_channel(unit, &a_cfg, &ch_a));
    ESP_ERROR_CHECK(pcnt_new_channel(unit, &b_cfg, &ch_b));

    /* Quadrature x4: edge on one line, direction from the other's level */
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(ch_a,
                    PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(ch_a,
                    PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(ch_b,
                    PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(ch_b,
                    PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    /* Watch points at the limits make accum_count work */
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(unit, PCNT_LIMIT));
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(unit, -PCNT_LIMIT));

    ESP_ERROR_CHECK(pcnt_unit_enable(unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(unit));
    ESP_ERROR_CHECK(pcnt_unit_start(unit));
    return unit;
}

static void gpio_init(void)
{
    const gpio_config_t in_cfg =
    {
        .pin_bit_mask = (1ULL << PIN_LAMP_BTN) | (1ULL << PIN_ENC_SW),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
    };
    const gpio_config_t led_cfg =
    {
        .pin_bit_mask = (1ULL << PIN_BOARD_LED),
        .mode         = GPIO_MODE_OUTPUT,
    };

    ESP_ERROR_CHECK(gpio_config(&in_cfg));
    ESP_ERROR_CHECK(gpio_config(&led_cfg));
    ESP_ERROR_CHECK(gpio_set_level(PIN_BOARD_LED, 0));
}

/** @return true once per press (debounced falling edge). */
static bool button_pressed(button_t *p_btn)
{
    const bool b_raw = (0 == gpio_get_level(p_btn->pin));   /* active low */
    bool       b_event = false;

    if (b_raw == p_btn->b_pressed)
    {
        p_btn->same = 0u;
    }
    else if (++p_btn->same >= DEBOUNCE_SAMPLES)
    {
        p_btn->b_pressed = b_raw;
        p_btn->same      = 0u;
        b_event          = b_raw;
    }
    else
    {
        /* still bouncing / not yet stable */
    }
    return b_event;
}

void app_main(void)
{
    button_t           lamp_btn = { .pin = PIN_LAMP_BTN };
    button_t           enc_sw   = { .pin = PIN_ENC_SW };
    pcnt_unit_handle_t enc;
    int                ref    = 0;     /* count at the last whole step */
    int                volume = 10;
    bool               b_lamp = false;
    bool               b_mute = false;

    gpio_init();
    enc = encoder_init();
    ESP_LOGI(TAG, "ready: lamp off, volume %d/%d, unmuted", volume, VOL_MAX);

    for (;;)
    {
        int count;
        int steps = 0;

        if (button_pressed(&lamp_btn))
        {
            b_lamp = !b_lamp;
            ESP_ERROR_CHECK(gpio_set_level(PIN_BOARD_LED, b_lamp ? 1 : 0));
            ESP_LOGI(TAG, "lamp %s", b_lamp ? "ON" : "off");
        }
        if (button_pressed(&enc_sw))
        {
            b_mute = !b_mute;
            ESP_LOGI(TAG, "%s", b_mute ? "MUTE" : "unmute");
        }

        ESP_ERROR_CHECK(pcnt_unit_get_count(enc, &count));
        while ((count - ref) >= COUNTS_PER_STEP)
        {
            ref += COUNTS_PER_STEP;
            steps++;
        }
        while ((ref - count) >= COUNTS_PER_STEP)
        {
            ref -= COUNTS_PER_STEP;
            steps--;
        }
        if (0 != steps)
        {
            const int old = volume;

            volume += steps;
            volume  = (volume > VOL_MAX) ? VOL_MAX : ((volume < 0) ? 0 : volume);
            ESP_LOGI(TAG, "volume %2d -> %2d  (%+d step, raw count %d)%s",
                     old, volume, steps, count, b_mute ? "  [muted]" : "");
        }

        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
}
