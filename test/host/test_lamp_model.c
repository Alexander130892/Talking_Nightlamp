/*
 * Executable specification for lamp_model (ADR 0007).
 */

#include "test.h"
#include "lamp_model.h"

#define N (12u)

static pixel_grbw_t s_f[N];

static uint32_t total_w(void)
{
    uint32_t sum = 0u;

    for (size_t i = 0u; i < N; i++)
    {
        sum += s_f[i].w;
    }
    return sum;
}

static uint32_t lit(void)
{
    uint32_t n = 0u;

    for (size_t i = 0u; i < N; i++)
    {
        n += (s_f[i].w > 0u) ? 1u : 0u;
    }
    return n;
}

TEST(level_zero_is_dark)
{
    lamp_model_render(0u, s_f, N);
    CHECK_EQ(0, total_w());
}

TEST(full_level_is_full_warm_white)
{
    lamp_model_render(LAMP_LEVEL_MAX_PM, s_f, N);
    for (size_t i = 0u; i < N; i++)
    {
        CHECK_EQ(255, s_f[i].w);
    }
}

TEST(only_white_channel_is_used)
{
    for (uint16_t lv = 0u; lv <= LAMP_LEVEL_MAX_PM; lv++)
    {
        lamp_model_render(lv, s_f, N);
        for (size_t i = 0u; i < N; i++)
        {
            CHECK_EQ(0, s_f[i].r);
            CHECK_EQ(0, s_f[i].g);
            CHECK_EQ(0, s_f[i].b);
        }
    }
}

TEST(total_light_is_monotonic)
{
    uint32_t prev = 0u;

    for (uint16_t lv = 0u; lv <= LAMP_LEVEL_MAX_PM; lv++)
    {
        lamp_model_render(lv, s_f, N);
        CHECK(total_w() >= prev);
        prev = total_w();
    }
}

TEST(lowest_nonzero_level_is_one_led_at_one)
{
    uint16_t lv = 1u;

    lamp_model_render(lv, s_f, N);
    while ((0u == total_w()) && (lv < LAMP_LEVEL_MAX_PM))
    {
        lv++;
        lamp_model_render(lv, s_f, N);
    }
    CHECK(total_w() > 0u);
    CHECK_EQ(1, lit());
    CHECK_EQ(1, total_w());
}

TEST(sub_step_range_uses_12_6_3_1_leds)
{
    int seen[N + 1u] = {0};

    for (uint16_t lv = 0u; lv <= LAMP_LEVEL_MAX_PM; lv++)
    {
        lamp_model_render(lv, s_f, N);
        if ((lit() > 0u) && (total_w() == lit())) /* all lit LEDs at W=1 */
        {
            seen[lit()] = 1;
        }
    }
    CHECK(seen[1] && seen[3] && seen[6] && seen[12]);
}

TEST(partial_patterns_are_evenly_spread)
{
    for (uint16_t lv = 0u; lv <= LAMP_LEVEL_MAX_PM; lv++)
    {
        uint32_t n;
        size_t   first = N;

        lamp_model_render(lv, s_f, N);
        n = lit();
        if ((n == 0u) || (n == N))
        {
            continue;
        }
        for (size_t i = 0u; i < N; i++)
        {
            if (s_f[i].w > 0u)
            {
                if (first == N)
                {
                    first = i;
                }
                /* lit LEDs sit every N/n positions */
                CHECK_EQ(0, (i - first) % (N / n));
            }
        }
    }
}

TEST(level_above_max_is_clamped)
{
    lamp_model_render(5000u, s_f, N);
    CHECK_EQ(255u * N, total_w());
}

static void run_suite(void)
{
    RUN(level_zero_is_dark);
    RUN(full_level_is_full_warm_white);
    RUN(only_white_channel_is_used);
    RUN(total_light_is_monotonic);
    RUN(lowest_nonzero_level_is_one_led_at_one);
    RUN(sub_step_range_uses_12_6_3_1_leds);
    RUN(partial_patterns_are_evenly_spread);
    RUN(level_above_max_is_clamped);
}
