/**
 * @file  lamp_strip.h
 * @brief SK6812 RGBW output through RMT (GRBW, 32 bits per pixel).
 *
 * Rationale: ADR 0006. Proven in bring-up 09. The caller passes a frame that
 * has already been through lamp_model, lamp_tint and power_limit.
 *
 * Timing: 0-bit 0.3/0.9 us, 1-bit 0.6/0.6 us, MSB first, wire order GRBW.
 * Reset (>= 80 us low) is the gap between frames; callers send at most
 * every few ms, so no explicit reset delay is needed.
 */

#ifndef LAMP_STRIP_H
#define LAMP_STRIP_H

#include <stddef.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "pixel.h"

typedef struct
{
    gpio_num_t data_gpio;   /* to 74AHCT125 1A, 5.1k pull-down on the board */
    size_t     n_pixels;
} lamp_strip_config_t;

/** Configure RMT and send an all-off frame. */
esp_err_t lamp_strip_init(const lamp_strip_config_t *p_cfg);

/**
 * Transmit one frame and wait until the last bit has left the pin (~0.6 ms
 * for 12 pixels). n must equal the configured n_pixels.
 */
esp_err_t lamp_strip_show(const pixel_grbw_t *p_frame, size_t n);

#endif /* LAMP_STRIP_H */
