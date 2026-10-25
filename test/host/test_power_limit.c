/*
 * Executable specification for power_limit (ADR 0008, power-budget.md).
 */

#include <string.h>
#include "test.h"
#include "power_limit.h"

#define N (12u)

static pixel_grbw_t s_f[N];

static void fill(uint8_t g, uint8_t r, uint8_t b, uint8_t w)
{
    for (size_t i = 0u; i < N; i++)
    {
        s_f[i].g = g;
        s_f[i].r = r;
        s_f[i].b = b;
        s_f[i].w = w;
    }
}

TEST(dark_frame_draws_nothing)
{
    fill(0u, 0u, 0u, 0u);
    CHECK_EQ(0, power_limit_estimate_ma(s_f, N));
}

TEST(full_rgbw_estimate_is_960_ma)
{
    fill(255u, 255u, 255u, 255u);
    CHECK_EQ(12 * 4 * 20, power_limit_estimate_ma(s_f, N));
}

TEST(estimate_rounds_up)
{
    fill(0u, 0u, 0u, 0u);
    s_f[0].w = 1u; /* 20/255 mA */
    CHECK_EQ(1, power_limit_estimate_ma(s_f, N));
}

TEST(frame_within_budget_is_unchanged)
{
    pixel_grbw_t ref[N];

    fill(0u, 0u, 0u, 100u); /* 12 * 100/255 * 20 ~ 94 mA */
    (void)memcpy(ref, s_f, sizeof(ref));
    power_limit_apply(s_f, N, POWER_LIMIT_BUDGET_MA);
    CHECK_EQ(0, memcmp(ref, s_f, sizeof(ref)));
}

TEST(over_budget_frame_is_scaled_into_budget)
{
    fill(255u, 255u, 255u, 255u);
    power_limit_apply(s_f, N, POWER_LIMIT_BUDGET_MA);
    CHECK(power_limit_estimate_ma(s_f, N) <= POWER_LIMIT_BUDGET_MA);
}

TEST(scaling_keeps_channels_and_never_increases)
{
    fill(0u, 200u, 100u, 0u); /* purple-ish, 12 * (200+100)/255 * 20 mA */
    power_limit_apply(s_f, N, 100u);
    for (size_t i = 0u; i < N; i++)
    {
        CHECK(s_f[i].r <= 200u);
        CHECK(s_f[i].b <= 100u);
        CHECK_EQ(0, s_f[i].g);
        CHECK_EQ(0, s_f[i].w);
        /* ratio r:b stays roughly 2:1 */
        CHECK((s_f[i].r >= (2u * s_f[i].b) - 2u) &&
              (s_f[i].r <= (2u * s_f[i].b) + 2u));
    }
    CHECK(power_limit_estimate_ma(s_f, N) <= 100u);
}

TEST(scaling_does_not_go_to_black_needlessly)
{
    fill(0u, 0u, 0u, 255u); /* 240 mA, budget 150 */
    power_limit_apply(s_f, N, POWER_LIMIT_BUDGET_MA);
    CHECK(power_limit_estimate_ma(s_f, N) >= POWER_LIMIT_BUDGET_MA - 12u);
}

static void run_suite(void)
{
    RUN(dark_frame_draws_nothing);
    RUN(full_rgbw_estimate_is_960_ma);
    RUN(estimate_rounds_up);
    RUN(frame_within_budget_is_unchanged);
    RUN(over_budget_frame_is_scaled_into_budget);
    RUN(scaling_keeps_channels_and_never_increases);
    RUN(scaling_does_not_go_to_black_needlessly);
}
