/**
 * @file  lamp_model.c
 * @brief Brightness level -> warm-white frame (ADR 0007).
 *
 * 1. Gamma: perceived level (0..1000 permille) -> physical light,
 *    light = level^2.2, via a lookup table with linear interpolation.
 * 2. Total light T = light * n * 255 (sum of W over all pixels), in
 *    fixed point with 8 fractional bits.
 * 3. T >= n: all pixels lit at W = round(T / n).
 *    T <  n: fewer pixels at W = 1, evenly spread (12 -> 6 -> 3 -> 1),
 *    giving extra steps below what 8-bit PWM can do.
 *
 * Designed for n = 12. Other n work if n is divisible by 4; otherwise the
 * spread of the partial patterns is only approximately even.
 *
 * Specification: test/host/test_lamp_model.c
 */

#include <string.h>
#include "lamp_model.h"

#define W_MAX        (255u)
#define FRAC_BITS    (8u)                  /* T is stored as T * 256 */
#define ONE_Q8       (1u << FRAC_BITS)
#define LUT_STEPS    (32u)

/*
 * x^2.2 for x = k/32, k = 0..32, scaled to 0..65535. Regenerate with:
 * python3 -c "print([round(((k/32)**2.2)*65535) for k in range(33)])"
 */
static const uint16_t GAMMA_LUT[LUT_STEPS + 1u] =
{
        0u,    32u,   147u,   359u,   676u,  1104u,  1648u,  2314u,
     3104u,  4022u,  5072u,  6255u,  7574u,  9033u, 10632u, 12375u,
    14263u, 16298u, 18482u, 20816u, 23303u, 25943u, 28739u, 31692u,
    34802u, 38072u, 41503u, 45097u, 48853u, 52774u, 56860u, 61114u,
    65535u,
};

/** Gamma-corrected light for a level in permille, 0..65535. */
static uint32_t gamma_light(uint16_t level_pm)
{
    const uint32_t pos  = (uint32_t)level_pm * LUT_STEPS;  /* 0..32000 */
    const uint32_t idx  = pos / LAMP_LEVEL_MAX_PM;         /* 0..32 */
    const uint32_t frac = pos % LAMP_LEVEL_MAX_PM;         /* 0..999 */
    uint32_t       light = GAMMA_LUT[idx];

    if (idx < LUT_STEPS)
    {
        const uint32_t next = GAMMA_LUT[idx + 1u];

        light += ((next - light) * frac) / LAMP_LEVEL_MAX_PM;
    }
    return light;
}

/** Light pixels 0, stride, 2*stride, ... at value w. */
static void light_spread(pixel_grbw_t *p_frame, size_t n, size_t count,
                         uint8_t w)
{
    const size_t stride = n / count;

    for (size_t i = 0u; i < count; i++)
    {
        p_frame[i * stride].w = w;
    }
}

void lamp_model_render(uint16_t level_pm, pixel_grbw_t *p_frame, size_t n)
{
    uint64_t t_q8;   /* total light T * 256 */

    (void)memset(p_frame, 0, n * sizeof(*p_frame));

    if (level_pm > LAMP_LEVEL_MAX_PM)
    {
        level_pm = LAMP_LEVEL_MAX_PM;
    }
    if ((0u == level_pm) || (0u == n))
    {
        return;
    }

    /* T * 256 = light/65536 * n * 255 * 256 = light * n * 255 / 256 */
    t_q8 = ((uint64_t)gamma_light(level_pm) * n * W_MAX) >> FRAC_BITS;

    if (t_q8 >= ((uint64_t)n * ONE_Q8))
    {
        /* Normal range: all pixels, W = round(T / n), max 255 */
        uint64_t w = (t_q8 + ((uint64_t)n * ONE_Q8 / 2u)) /
                     ((uint64_t)n * ONE_Q8);

        light_spread(p_frame, n, n, (uint8_t)((w > W_MAX) ? W_MAX : w));
    }
    else if (t_q8 >= ((uint64_t)(n / 2u) * ONE_Q8))
    {
        light_spread(p_frame, n, n / 2u, 1u);          /* 12 -> 6 */
    }
    else if (t_q8 >= ((uint64_t)(n / 4u) * ONE_Q8))
    {
        light_spread(p_frame, n, n / 4u, 1u);          /* 6 -> 3 */
    }
    else
    {
        light_spread(p_frame, n, 1u, 1u);              /* lowest glow */
    }
}