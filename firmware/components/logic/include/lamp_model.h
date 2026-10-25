/**
 * @file  lamp_model.h
 * @brief Brightness level -> warm-white frame, including sub-step dimming.
 *
 * Rationale: ADR 0007. Specification: test/host/test_lamp_model.c.
 *
 * Contract
 *  - level_pm is brightness in permille: 0 = off, 1000 = full.
 *  - level 0 -> every channel of every pixel is 0.
 *  - Only the W channel is used; R, G, B stay 0.
 *  - Total light (sum of W over all pixels) never decreases as level_pm
 *    increases (monotonic).
 *  - Above the low range, all pixels carry the same gamma-corrected value.
 *  - At the bottom of the range, fewer pixels are lit at W = 1, spread
 *    evenly around the ring: 12 -> 6 -> 3 -> 1. The lowest non-zero level
 *    lights exactly one pixel at W = 1.
 */

#ifndef LAMP_MODEL_H
#define LAMP_MODEL_H

#include <stddef.h>
#include <stdint.h>
#include "pixel.h"

#define LAMP_LEVEL_MAX_PM (1000u)

/**
 * Render a warm-white frame.
 * @param level_pm  0..LAMP_LEVEL_MAX_PM, larger values are clamped
 * @param p_frame   output, n pixels
 * @param n         number of pixels (12 in this build; must be > 0)
 */
void lamp_model_render(uint16_t level_pm, pixel_grbw_t *p_frame, size_t n);

#endif /* LAMP_MODEL_H */
