/**
 * @file  pixel.h
 * @brief One SK6812 RGBW pixel, in wire order (GRBW).
 */

#ifndef PIXEL_H
#define PIXEL_H

#include <stdint.h>

typedef struct
{
    uint8_t g;
    uint8_t r;
    uint8_t b;
    uint8_t w;
} pixel_grbw_t;

#endif /* PIXEL_H */
