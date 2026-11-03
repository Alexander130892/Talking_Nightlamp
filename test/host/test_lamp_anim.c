/*
 * Executable specification for lamp_anim (F1, F2, F8, F9, F10).
 */

#include "test.h"
#include "lamp_anim.h"

static lamp_anim_t s_a;

TEST(starts_off)
{
    lamp_anim_init(&s_a);
    CHECK_EQ(0, lamp_anim_level(&s_a, 0u));
    CHECK_EQ(0, lamp_anim_level(&s_a, 123456u));
}

TEST(zero_duration_sets_at_once)
{
    lamp_anim_init(&s_a);
    lamp_anim_fade_to(&s_a, 700u, 0u, 10u);
    CHECK_EQ(700, lamp_anim_level(&s_a, 10u));
}

TEST(fade_is_linear_and_ends_on_target)
{
    lamp_anim_init(&s_a);
    lamp_anim_fade_to(&s_a, 1000u, 1000u, 0u);
    CHECK_EQ(0, lamp_anim_level(&s_a, 0u));
    CHECK_EQ(500, lamp_anim_level(&s_a, 500u));
    CHECK_EQ(1000, lamp_anim_level(&s_a, 1000u));
    CHECK_EQ(1000, lamp_anim_level(&s_a, 5000u));
}

TEST(fade_down_works)
{
    lamp_anim_init(&s_a);
    lamp_anim_fade_to(&s_a, 800u, 0u, 0u);
    lamp_anim_fade_to(&s_a, 0u, 400u, 0u);
    CHECK_EQ(400, lamp_anim_level(&s_a, 200u));
    CHECK_EQ(0, lamp_anim_level(&s_a, 400u));
}

TEST(interrupted_fade_starts_from_current_level_no_jump)
{
    uint16_t before;
    uint16_t after;

    lamp_anim_init(&s_a);
    lamp_anim_fade_to(&s_a, 1000u, 10000u, 0u);
    before = lamp_anim_level(&s_a, 3000u);          /* 300 */
    lamp_anim_fade_to(&s_a, 0u, 2000u, 3000u);
    after = lamp_anim_level(&s_a, 3000u);
    CHECK_EQ(300, before);
    CHECK_EQ(before, after);
    CHECK_EQ(150, lamp_anim_level(&s_a, 4000u));
    CHECK_EQ(0, lamp_anim_level(&s_a, 5000u));
}

TEST(target_is_clamped)
{
    lamp_anim_init(&s_a);
    lamp_anim_fade_to(&s_a, 5000u, 0u, 0u);
    CHECK_EQ(LAMP_ANIM_MAX_PM, lamp_anim_level(&s_a, 0u));
}

TEST(pulse_rises_from_off_and_returns)
{
    lamp_anim_init(&s_a);
    lamp_anim_pulse(&s_a, 100u);
    CHECK_EQ(0, lamp_anim_level(&s_a, 100u));
    CHECK_EQ(LAMP_ANIM_PULSE_PM,
             lamp_anim_level(&s_a, 100u + (LAMP_ANIM_PULSE_MS / 2u)));
    CHECK_EQ(0, lamp_anim_level(&s_a, 100u + LAMP_ANIM_PULSE_MS));
    CHECK(!s_a.b_pulse);
}

TEST(pulse_dips_when_bright)
{
    lamp_anim_init(&s_a);
    lamp_anim_fade_to(&s_a, 1000u, 0u, 0u);
    lamp_anim_pulse(&s_a, 0u);
    CHECK_EQ(1000 - LAMP_ANIM_PULSE_PM,
             lamp_anim_level(&s_a, LAMP_ANIM_PULSE_MS / 2u));
}

TEST(pulse_envelope_is_smooth)
{
    /* smoothstep: near the ends it moves much less than a straight ramp */
    lamp_anim_init(&s_a);
    lamp_anim_pulse(&s_a, 0u);
    CHECK(lamp_anim_level(&s_a, LAMP_ANIM_PULSE_MS / 20u) < (LAMP_ANIM_PULSE_PM / 10u));
}

TEST(pulse_does_not_change_base)
{
    lamp_anim_init(&s_a);
    lamp_anim_fade_to(&s_a, 300u, 0u, 0u);
    lamp_anim_pulse(&s_a, 0u);
    CHECK_EQ(300, lamp_anim_base(&s_a, 400u));
}

TEST(breathe_dips_to_depth_mid_period)
{
    lamp_anim_init(&s_a);
    lamp_anim_fade_to(&s_a, 1000u, 0u, 0u);
    lamp_anim_breathe(&s_a, true, 0u);
    CHECK_EQ(1000, lamp_anim_level(&s_a, 0u));
    CHECK_EQ(1000 - LAMP_ANIM_BREATHE_DEPTH_PM,
             lamp_anim_level(&s_a, LAMP_ANIM_BREATHE_MS / 2u));
    CHECK_EQ(1000, lamp_anim_level(&s_a, LAMP_ANIM_BREATHE_MS));
}

TEST(breathe_stop_finishes_current_dip)
{
    lamp_anim_init(&s_a);
    lamp_anim_fade_to(&s_a, 1000u, 0u, 0u);
    lamp_anim_breathe(&s_a, true, 0u);
    lamp_anim_breathe(&s_a, false, LAMP_ANIM_BREATHE_MS / 4u);
    /* still dipping at mid-period */
    CHECK_EQ(1000 - LAMP_ANIM_BREATHE_DEPTH_PM,
             lamp_anim_level(&s_a, LAMP_ANIM_BREATHE_MS / 2u));
    /* finished at the period boundary, then stays at base */
    CHECK_EQ(1000, lamp_anim_level(&s_a, LAMP_ANIM_BREATHE_MS));
    CHECK(!s_a.b_breathe);
    CHECK_EQ(1000, lamp_anim_level(&s_a, (LAMP_ANIM_BREATHE_MS * 3u) / 2u));
}

TEST(breathe_restart_while_stopping_keeps_breathing)
{
    lamp_anim_init(&s_a);
    lamp_anim_fade_to(&s_a, 1000u, 0u, 0u);
    lamp_anim_breathe(&s_a, true, 0u);
    lamp_anim_breathe(&s_a, false, 100u);
    lamp_anim_breathe(&s_a, true, 200u);
    (void)lamp_anim_level(&s_a, LAMP_ANIM_BREATHE_MS + 1u);
    CHECK(s_a.b_breathe);
}

TEST(works_across_counter_wrap)
{
    const uint32_t t0 = 0xFFFFFF00u;

    lamp_anim_init(&s_a);
    lamp_anim_fade_to(&s_a, 1000u, 512u, t0);
    CHECK_EQ(500, lamp_anim_level(&s_a, t0 + 256u));   /* wraps past 0 */
    CHECK_EQ(1000, lamp_anim_level(&s_a, t0 + 600u));
}

static void run_suite(void)
{
    RUN(starts_off);
    RUN(zero_duration_sets_at_once);
    RUN(fade_is_linear_and_ends_on_target);
    RUN(fade_down_works);
    RUN(interrupted_fade_starts_from_current_level_no_jump);
    RUN(target_is_clamped);
    RUN(pulse_rises_from_off_and_returns);
    RUN(pulse_dips_when_bright);
    RUN(pulse_envelope_is_smooth);
    RUN(pulse_does_not_change_base);
    RUN(breathe_dips_to_depth_mid_period);
    RUN(breathe_stop_finishes_current_dip);
    RUN(breathe_restart_while_stopping_keeps_breathing);
    RUN(works_across_counter_wrap);
}
