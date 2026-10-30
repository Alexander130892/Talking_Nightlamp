/**
 * @file  09_led.c
 * @brief Bring-up step 9: RMT -> 74AHCT125 -> SK6812 RGBW strip (12 LEDs).
 *
 * Bit timing (SK6812 datasheet), 10 MHz RMT clock = 0.1 us per tick:
 *   0-bit: 0.3 us high, 0.9 us low
 *   1-bit: 0.6 us high, 0.6 us low      -> 1.2 us per bit
 *   reset: line low >= 80 us (here: the gap between frames, >= 10 ms)
 * Byte order on the wire: G, R, B, W, MSB first.
 *
 * Test sequence (repeats):
 *   1. all LEDs red, green, blue, white (1 s each)  -> byte order
 *   2. one white LED walking 0..11                  -> every LED gets data
 *   3. all warm white breathing up and down         -> smooth, no flicker
 *
 * Level is capped at LEVEL_MAX: 12 LEDs x 1 die x 20 mA x 32/255 ~ 30 mA,
 * safe on laptop USB.
 */

#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rmt_tx.h"
#include "esp_timer.h"
#include "esp_log.h"

#define PIN_LED_DATA     (32)
#define LED_COUNT        (12u)
#define BYTES_PER_LED    (4u)          /* G R B W */
#define RMT_RES_HZ       (10000000u)   /* 0.1 us per tick */
#define RMT_MEM_SYMBOLS  (128u)        /* 2 blocks: fewer refill interrupts */
#define LEVEL_MAX        (32u)

static const char *TAG = "led";

static rmt_channel_handle_t s_chan;
static rmt_encoder_handle_t s_enc;
static uint8_t              s_px[LED_COUNT * BYTES_PER_LED];

static void strip_init(void)
{
    const rmt_tx_channel_config_t chan_cfg =
    {
        .gpio_num          = PIN_LED_DATA,
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .resolution_hz     = RMT_RES_HZ,
        .mem_block_symbols = RMT_MEM_SYMBOLS,
        .trans_queue_depth = 4,
    };
    /* Durations in ticks of 0.1 us */
    const rmt_bytes_encoder_config_t enc_cfg =
    {
        .bit0  = { .level0 = 1, .duration0 = 3, .level1 = 0, .duration1 = 9 },
        .bit1  = { .level0 = 1, .duration0 = 6, .level1 = 0, .duration1 = 6 },
        .flags = { .msb_first = 1 },
    };

    ESP_ERROR_CHECK(rmt_new_tx_channel(&chan_cfg, &s_chan));
    ESP_ERROR_CHECK(rmt_new_bytes_encoder(&enc_cfg, &s_enc));
    ESP_ERROR_CHECK(rmt_enable(s_chan));
}

/** Send s_px and wait until the last bit has left the pin. */
static void strip_show(void)
{
    const rmt_transmit_config_t tx_cfg = { .loop_count = 0 };   /* eot low */

    ESP_ERROR_CHECK(rmt_transmit(s_chan, s_enc, s_px, sizeof(s_px), &tx_cfg));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(s_chan, portMAX_DELAY));
}

static void set_led(uint32_t i, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    uint8_t *p_led = &s_px[i * BYTES_PER_LED];

    p_led[0] = g;
    p_led[1] = r;
    p_led[2] = b;
    p_led[3] = w;
}

static void set_all(uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    for (uint32_t i = 0u; i < LED_COUNT; i++)
    {
        set_led(i, r, g, b, w);
    }
}

/** Time one frame on the wire, to compare with the prediction. */
static void measure_frame(void)
{
    const int64_t t0 = esp_timer_get_time();

    strip_show();
    ESP_LOGI(TAG, "frame: %u bytes, %lld us (incl. driver overhead)",
             (unsigned)sizeof(s_px), esp_timer_get_time() - t0);
}

static void test_colours(void)
{
    static const struct { const char *p_name; uint8_t r, g, b, w; } k_steps[] =
    {
        { "red",   LEVEL_MAX, 0u, 0u, 0u },
        { "green", 0u, LEVEL_MAX, 0u, 0u },
        { "blue",  0u, 0u, LEVEL_MAX, 0u },
        { "white", 0u, 0u, 0u, LEVEL_MAX },
    };

    for (uint32_t i = 0u; i < (sizeof(k_steps) / sizeof(k_steps[0])); i++)
    {
        ESP_LOGI(TAG, "all %s", k_steps[i].p_name);
        set_all(k_steps[i].r, k_steps[i].g, k_steps[i].b, k_steps[i].w);
        strip_show();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void test_walk(void)
{
    ESP_LOGI(TAG, "walk 0..%u", (unsigned)(LED_COUNT - 1u));
    for (uint32_t i = 0u; i < LED_COUNT; i++)
    {
        set_all(0u, 0u, 0u, 0u);
        set_led(i, 0u, 0u, 0u, LEVEL_MAX);
        strip_show();
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}

/** Linear ramp, 40 ms per step (25 fps). No gamma yet: low end will
 *  look steppy, which is exactly what ADR 0007 is about. */
static void test_breathe(void)
{
    ESP_LOGI(TAG, "breathe 0..%u..0", (unsigned)LEVEL_MAX);
    for (uint32_t v = 0u; v <= LEVEL_MAX; v++)
    {
        set_all(0u, 0u, 0u, (uint8_t)v);
        strip_show();
        vTaskDelay(pdMS_TO_TICKS(40));
    }
    for (uint32_t v = LEVEL_MAX; v > 0u; v--)
    {
        set_all(0u, 0u, 0u, (uint8_t)(v - 1u));
        strip_show();
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}

void app_main(void)
{
    strip_init();

    /* First thing after init: a defined all-off frame */
    set_all(0u, 0u, 0u, 0u);
    measure_frame();

    for (;;)
    {
        test_colours();
        test_walk();
        test_breathe();
        set_all(0u, 0u, 0u, 0u);
        strip_show();
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
