/**
 * @file  lamp_tint.h
 * @brief Card colour mixed into the warm white (F15).
 *
 * Pure logic, host-tested (test/host/test_lamp_tint.c).
 *
 * Contract for lamp_tint_apply(), per pixel with input white W:
 *  - R, G, B = colour(hue) * W * strength_pm / (255 * 1000), rounded down.
 *  - W' = W - (R + G + B): the colour REPLACES part of the white.
 *  - So R + G + B + W' = W exactly: the estimated current (power_limit) is
 *    unchanged by the tint, at any hue and strength.
 *  - strength_pm = 0 leaves the pixel plain warm white.
 *  - strength_pm = 1000 gives (almost) pure colour; up to 2 counts of white
 *    remain from rounding.
 *  - Small W rounds the colour to 0: the night glow stays warm white and a
 *    lit pixel never goes dark because of the tint.
 *  - Any R, G, B already in the frame are overwritten.
 *  - Hue is a 0..255 colour wheel: 0 red, 85 green, 170 blue.
 */

#ifndef LAMP_TINT_H
#define LAMP_TINT_H

#include <stddef.h>
#include <stdint.h>
#include "pixel.h"

#define LAMP_TINT_DEFAULT_PM (500u)

void lamp_tint_apply(pixel_grbw_t *p_frame, size_t n, uint8_t hue,
                     uint16_t strength_pm);

#endif /* LAMP_TINT_H */
