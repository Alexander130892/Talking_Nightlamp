/**
 * @file  lamp_strip.c
 * @brief SK6812 RMT driver (see lamp_strip.h).
 */

#include "lamp_strip.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "driver/rmt_tx.h"
#include "esp_check.h"

#define RMT_RES_HZ      (10000000u)   /* 0.1 us per tick */
#define RMT_MEM_SYMBOLS (128u)        /* 2 blocks: fewer refill interrupts */
#define MAX_PIXELS      (64u)

static const char           *TAG = "lamp_strip";
static rmt_channel_handle_t  s_chan;
static rmt_encoder_handle_t  s_enc;
static size_t                s_n;

esp_err_t lamp_strip_init(const lamp_strip_config_t *p_cfg)
{
    static pixel_grbw_t off[MAX_PIXELS];   /* zero-initialised */
    rmt_tx_channel_config_t chan_cfg =
    {
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .resolution_hz     = RMT_RES_HZ,
        .mem_block_symbols = RMT_MEM_SYMBOLS,
        .trans_queue_depth = 4,
    };
    const rmt_bytes_encoder_config_t enc_cfg =
    {
        .bit0  = { .level0 = 1, .duration0 = 3, .level1 = 0, .duration1 = 9 },
        .bit1  = { .level0 = 1, .duration0 = 6, .level1 = 0, .duration1 = 6 },
        .flags = { .msb_first = 1 },
    };

    ESP_RETURN_ON_FALSE(NULL != p_cfg, ESP_ERR_INVALID_ARG, TAG, "no config");
    ESP_RETURN_ON_FALSE((p_cfg->n_pixels > 0u) && (p_cfg->n_pixels <= MAX_PIXELS),
                        ESP_ERR_INVALID_ARG, TAG, "n_pixels");
    ESP_RETURN_ON_FALSE(NULL == s_chan, ESP_ERR_INVALID_STATE, TAG,
                        "already initialised");

    chan_cfg.gpio_num = p_cfg->data_gpio;
    ESP_RETURN_ON_ERROR(rmt_new_tx_channel(&chan_cfg, &s_chan), TAG, "channel");
    ESP_RETURN_ON_ERROR(rmt_new_bytes_encoder(&enc_cfg, &s_enc), TAG, "encoder");
    ESP_RETURN_ON_ERROR(rmt_enable(s_chan), TAG, "enable");
    s_n = p_cfg->n_pixels;

    /* First thing on the wire: a defined all-off frame */
    return lamp_strip_show(off, s_n);
}

esp_err_t lamp_strip_show(const pixel_grbw_t *p_frame, size_t n)
{
    const rmt_transmit_config_t tx_cfg = { .loop_count = 0 };   /* eot low */

    ESP_RETURN_ON_FALSE(NULL != s_chan, ESP_ERR_INVALID_STATE, TAG, "not initialised");
    ESP_RETURN_ON_FALSE((NULL != p_frame) && (n == s_n), ESP_ERR_INVALID_ARG,
                        TAG, "frame");
    /* pixel_grbw_t is 4 bytes in wire order: send the array as-is */
    ESP_RETURN_ON_ERROR(rmt_transmit(s_chan, s_enc, p_frame,
                                     n * sizeof(pixel_grbw_t), &tx_cfg),
                        TAG, "transmit");
    return rmt_tx_wait_all_done(s_chan, portMAX_DELAY);
}
