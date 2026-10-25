/**
 * @file  lamp_strip.h
 * @brief SK6812 RGBW output through RMT (GRBW, 32 bits per pixel).
 *
 * Rationale: ADR 0006. The caller passes a frame that has already been
 * through lamp_model and power_limit.
 */

#ifndef LAMP_STRIP_H
#define LAMP_STRIP_H

#include <stddef.h>
#include "esp_err.h"
#include "pixel.h"

/** Configure RMT on BOARD_LED_DATA and send an all-off frame. */
esp_err_t lamp_strip_init(size_t n_pixels);

/** Transmit one frame and wait for the latch. */
esp_err_t lamp_strip_show(const pixel_grbw_t *p_frame, size_t n);

#endif /* LAMP_STRIP_H */
