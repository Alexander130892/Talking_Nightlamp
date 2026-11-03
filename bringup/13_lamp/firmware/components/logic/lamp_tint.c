/**
 * @file  lamp_tint.c
 * @brief Card colour mixed into the white (see lamp_tint.h for the contract).
 */

#include "lamp_tint.h"

typedef struct
{
    uint32_t r;
    uint32_t g;
    uint32_t b;
} rgb_t;

/** Fully saturated colour on a 3-segment wheel, channels 0..255. */
static rgb_t wheel(uint8_t hue)
{
    const uint32_t h = hue;
    rgb_t          c;

    if (h < 85u)
    {
        c = (rgb_t){ 255u - (3u * h), 3u * h, 0u };
    }
    else if (h < 170u)
    {
        const uint32_t k = h - 85u;

        c = (rgb_t){ 0u, 255u - (3u * k), 3u * k };
    }
    else
    {
        const uint32_t k = h - 170u;

        c = (rgb_t){ 3u * k, 0u, 255u - (3u * k) };
    }
    return c;
}

/** colour * w * strength / (255 * 1000), always <= 255. */
static uint8_t tint(uint32_t colour, uint32_t w, uint32_t strength_pm)
{
    return (uint8_t)((colour * w * strength_pm) / (255u * 1000u));
}

void lamp_tint_apply(pixel_grbw_t *p_frame, size_t n, uint8_t hue,
                     uint16_t strength_pm)
{
    const rgb_t    c = wheel(hue);
    const uint32_t s = (strength_pm > 1000u) ? 1000u : strength_pm;

    for (size_t i = 0u; i < n; i++)
    {
        const uint32_t w = p_frame[i].w;
        const uint8_t  r = tint(c.r, w, s);
        const uint8_t  g = tint(c.g, w, s);
        const uint8_t  b = tint(c.b, w, s);

        /* r + g + b <= w * s / 1000 <= w, so this never underflows */
        p_frame[i].r = r;
        p_frame[i].g = g;
        p_frame[i].b = b;
        p_frame[i].w = (uint8_t)(w - ((uint32_t)r + g + b));
    }
}
