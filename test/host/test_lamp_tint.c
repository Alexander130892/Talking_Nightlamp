/*
 * Executable specification for lamp_tint (F15).
 */

#include "test.h"
#include "lamp_tint.h"

#define N (12u)

static pixel_grbw_t s_f[N];

static void fill_w(uint8_t w)
{
    for (size_t i = 0u; i < N; i++)
    {
        s_f[i] = (pixel_grbw_t){ .g = 9u, .r = 9u, .b = 9u, .w = w };
    }
}

static uint32_t sum(const pixel_grbw_t *p)
{
    return (uint32_t)p->r + p->g + p->b + p->w;
}

TEST(zero_strength_is_plain_white)
{
    fill_w(200u);
    lamp_tint_apply(s_f, N, 42u, 0u);
    CHECK_EQ(0, s_f[0].r);
    CHECK_EQ(0, s_f[0].g);
    CHECK_EQ(0, s_f[0].b);
    CHECK_EQ(200, s_f[0].w);
}

TEST(dark_pixel_stays_dark)
{
    fill_w(0u);
    lamp_tint_apply(s_f, N, 0u, 1000u);
    CHECK_EQ(0, sum(&s_f[3]));
}

TEST(total_is_conserved_for_every_hue_strength_and_level)
{
    for (uint32_t w = 0u; w < 256u; w += 17u)
    {
        for (uint32_t h = 0u; h < 256u; h += 5u)
        {
            for (uint32_t st = 0u; st <= 1000u; st += 125u)
            {
                fill_w((uint8_t)w);
                lamp_tint_apply(s_f, N, (uint8_t)h, (uint16_t)st);
                CHECK_EQ(w, sum(&s_f[0]));
            }
        }
    }
}

TEST(glow_stays_white_and_lit)
{
    fill_w(1u);
    lamp_tint_apply(s_f, N, 0u, LAMP_TINT_DEFAULT_PM);
    CHECK_EQ(1, s_f[0].w);
    CHECK_EQ(0, s_f[0].r);
    lamp_tint_apply(s_f, N, 0u, 1000u);        /* even at full strength */
    CHECK_EQ(1, sum(&s_f[0]));
}

TEST(half_strength_is_half_colour_half_white)
{
    fill_w(200u);
    lamp_tint_apply(s_f, N, 0u, 500u);          /* red */
    CHECK_EQ(100, s_f[0].r);
    CHECK_EQ(100, s_f[0].w);
}

TEST(full_strength_is_pure_colour)
{
    fill_w(255u);
    lamp_tint_apply(s_f, N, 0u, 1000u);
    CHECK_EQ(255, s_f[0].r);
    CHECK_EQ(0, s_f[0].w);
    fill_w(255u);
    lamp_tint_apply(s_f, N, 85u, 1000u);        /* green */
    CHECK_EQ(255, s_f[0].g);
    fill_w(255u);
    lamp_tint_apply(s_f, N, 170u, 1000u);       /* blue */
    CHECK_EQ(255, s_f[0].b);
    fill_w(255u);
    lamp_tint_apply(s_f, N, 42u, 1000u);        /* mixed hue: rounding */
    CHECK(s_f[0].w <= 2u);
}

static void run_suite(void)
{
    RUN(zero_strength_is_plain_white);
    RUN(dark_pixel_stays_dark);
    RUN(total_is_conserved_for_every_hue_strength_and_level);
    RUN(glow_stays_white_and_lit);
    RUN(half_strength_is_half_colour_half_white);
    RUN(full_strength_is_pure_colour);
}
