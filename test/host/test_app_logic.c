/*
 * Executable specification for app_logic: the requirements as scenarios.
 * Polls arrive every 150 ms like on the device.
 */

#include <string.h>
#include "test.h"
#include "app_logic.h"

#define POLL_MS (150u)

static app_logic_t s_app;
static uint32_t    s_now;

static card_poll_t card(uint8_t uid0, uint8_t story)
{
    card_poll_t p = { .b_present = true, .b_valid = (0u != story),
                      .uid = { uid0, 2u, 3u, 4u }, .uid_len = 4u,
                      .story_id = story, .hue = 42u };
    return p;
}

static const card_poll_t NO_CARD   = { 0 };
static const card_poll_t COLLISION = { .b_present = true };

/** Count commands of a type in a list; fill *p_cmd with the last one. */
static int count(const app_cmds_t *p_c, app_cmd_type_t type, app_cmd_t *p_cmd)
{
    int n = 0;

    for (uint8_t i = 0u; i < p_c->n; i++)
    {
        if (type == p_c->cmd[i].type)
        {
            n++;
            if (NULL != p_cmd)
            {
                *p_cmd = p_c->cmd[i];
            }
        }
    }
    return n;
}

/* Collected commands of a time span */
static app_cmds_t s_log[10000];
static uint32_t   s_log_n;

static void collect(app_cmds_t c)
{
    if (s_log_n < (sizeof(s_log) / sizeof(s_log[0])))
    {
        s_log[s_log_n++] = c;
    }
}

static int total(app_cmd_type_t type, app_cmd_t *p_last)
{
    int n = 0;

    for (uint32_t i = 0u; i < s_log_n; i++)
    {
        n += count(&s_log[i], type, p_last);
    }
    return n;
}

/** Feed the same poll for ms milliseconds, ticking in between. */
static void feed(const card_poll_t *p_poll, uint32_t ms)
{
    for (uint32_t t = 0u; t < ms; t += POLL_MS)
    {
        s_now += POLL_MS;
        collect(app_logic_poll(&s_app, p_poll, s_now));
        collect(app_logic_tick(&s_app, s_now));
    }
}

static void clear_log(void)
{
    s_log_n = 0u;
}

static void setup(bool b_sd_ok)
{
    s_now = 1000u;
    clear_log();
    collect(app_logic_init(&s_app, b_sd_ok, s_now));
}

static void press(app_input_t in, int8_t steps)
{
    collect(app_logic_input(&s_app, in, steps, s_now));
}

/* ---- F1 / F15: placing a card ---------------------------------------- */

TEST(card_starts_story_warms_lamp_with_card_colour)
{
    const card_poll_t c = card(1u, 7u);
    app_cmd_t         cmd;

    setup(true);
    feed(&c, 600u);
    CHECK_EQ(0, total(APP_CMD_AUDIO_PLAY, NULL));   /* light first */
    feed(&c, APP_STORY_DELAY_MS);
    CHECK_EQ(1, total(APP_CMD_AUDIO_PLAY, &cmd));
    CHECK_EQ(7, cmd.arg);
    CHECK_EQ(1, total(APP_CMD_LAMP_TINT, &cmd));
    CHECK_EQ(42, cmd.arg);
    CHECK_EQ(APP_TINT_PM, cmd.level_pm);
    CHECK_EQ(1, total(APP_CMD_LAMP_FADE, &cmd));
    CHECK_EQ(APP_LEVEL_FULL_PM, cmd.level_pm);
}

/* ---- F2 / F3 / F4 / F5: lifting the card ------------------------------ */

TEST(short_lift_breathes_and_restores_without_pause)
{
    const card_poll_t c = card(1u, 7u);
    app_cmd_t         cmd;

    setup(true);
    feed(&c, 600u);
    clear_log();
    feed(&NO_CARD, 1000u);
    feed(&c, 300u);
    CHECK_EQ(0, total(APP_CMD_AUDIO_PAUSE, NULL));
    CHECK_EQ(2, total(APP_CMD_LAMP_BREATHE, &cmd));
    CHECK_EQ(0, cmd.arg);                       /* last one: off */
}

TEST(long_lift_pauses_lamp_stays_full_and_resume_works)
{
    const card_poll_t c = card(1u, 7u);

    setup(true);
    feed(&c, 600u + APP_STORY_DELAY_MS);
    clear_log();
    feed(&NO_CARD, 4000u);
    CHECK_EQ(1, total(APP_CMD_AUDIO_PAUSE, NULL));
    CHECK_EQ(0, total(APP_CMD_LAMP_FADE, NULL));   /* stays full (3a) */
    clear_log();
    feed(&c, 600u);
    CHECK_EQ(1, total(APP_CMD_AUDIO_RESUME, NULL));
}

TEST(story_waits_for_the_light)
{
    const card_poll_t c = card(1u, 7u);

    setup(true);
    feed(&c, APP_STORY_DELAY_MS - POLL_MS);
    CHECK_EQ(0, total(APP_CMD_AUDIO_PLAY, NULL));
    feed(&c, 600u);
    CHECK_EQ(1, total(APP_CMD_AUDIO_PLAY, NULL));
}

TEST(paused_before_start_starts_on_return)
{
    const card_poll_t c = card(1u, 7u);
    app_cmd_t         cmd;

    setup(true);
    feed(&c, 600u);
    feed(&NO_CARD, 4000u);                      /* paused before the start */
    CHECK_EQ(0, total(APP_CMD_AUDIO_PLAY, NULL));
    clear_log();
    feed(&c, 600u);                             /* back: starts now */
    CHECK_EQ(1, total(APP_CMD_AUDIO_PLAY, &cmd));
    CHECK_EQ(7, cmd.arg);
    CHECK_EQ(0, total(APP_CMD_AUDIO_RESUME, NULL));
    clear_log();
    feed(&c, APP_STORY_DELAY_MS);
    CHECK_EQ(0, total(APP_CMD_AUDIO_PLAY, NULL));  /* not twice */
}

TEST(other_card_during_wait_replaces_story)
{
    const card_poll_t a = card(1u, 7u);
    const card_poll_t b = card(9u, 8u);
    app_cmd_t         cmd;

    setup(true);
    feed(&a, 3000u);
    feed(&NO_CARD, 300u);
    feed(&b, 600u + APP_STORY_DELAY_MS);
    CHECK_EQ(1, total(APP_CMD_AUDIO_PLAY, &cmd));
    CHECK_EQ(8, cmd.arg);
}

/* ---- F7: another card --------------------------------------------------- */

TEST(other_card_switches_story)
{
    const card_poll_t a = card(1u, 7u);
    const card_poll_t b = card(9u, 8u);
    app_cmd_t         cmd;

    setup(true);
    feed(&a, 600u + APP_STORY_DELAY_MS);
    clear_log();
    feed(&NO_CARD, 300u);
    feed(&b, 600u + APP_STORY_DELAY_MS);
    CHECK_EQ(1, total(APP_CMD_AUDIO_PLAY, &cmd));
    CHECK_EQ(8, cmd.arg);
}

/* ---- F8: story end ------------------------------------------------------ */

TEST(story_end_glows_holds_then_fades_off)
{
    const card_poll_t c = card(1u, 7u);
    app_cmd_t         cmd;

    setup(true);
    feed(&c, 600u);
    clear_log();
    collect(app_logic_story_end(&s_app, false, s_now));
    CHECK_EQ(1, total(APP_CMD_AUDIO_STOP, NULL));
    CHECK_EQ(1, total(APP_CMD_LAMP_FADE, &cmd));
    CHECK_EQ(APP_LEVEL_GLOW_PM, cmd.level_pm);
    CHECK_EQ(APP_GLOW_FADE_MS, cmd.dur_ms);

    clear_log();
    s_now += APP_GLOW_FADE_MS + APP_GLOW_HOLD_MS - 1000u;
    collect(app_logic_tick(&s_app, s_now));
    CHECK_EQ(0, total(APP_CMD_LAMP_FADE, NULL));   /* still glowing */
    s_now += 1000u;
    collect(app_logic_tick(&s_app, s_now));
    CHECK_EQ(1, total(APP_CMD_LAMP_FADE, &cmd));
    CHECK_EQ(0, cmd.level_pm);
    CHECK_EQ(APP_OFF_FADE_MS, cmd.dur_ms);
    s_now += APP_OFF_FADE_MS;
    collect(app_logic_tick(&s_app, s_now));
    CHECK_EQ(LAMP_MODE_OFF, s_app.lamp);
}

TEST(card_left_on_after_end_does_not_restart)
{
    const card_poll_t c = card(1u, 7u);

    setup(true);
    feed(&c, 600u);
    collect(app_logic_story_end(&s_app, false, s_now));
    clear_log();
    feed(&c, 3000u);
    CHECK_EQ(0, total(APP_CMD_AUDIO_PLAY, NULL));
}

TEST(pause_timeout_goes_to_glow)
{
    const card_poll_t c = card(1u, 7u);
    app_cmd_t         cmd;

    setup(true);
    feed(&c, 600u);
    feed(&NO_CARD, 4000u);                      /* paused */
    clear_log();
    feed(&NO_CARD, 10u * 60u * 1000u);          /* F6: 10 min */
    CHECK_EQ(1, total(APP_CMD_AUDIO_STOP, NULL));
    CHECK(total(APP_CMD_LAMP_FADE, &cmd) >= 1);
    CHECK_EQ(LAMP_MODE_GLOW, s_app.lamp);
}

/* ---- F9 / F13: unknown card, two cards ---------------------------------- */

TEST(unknown_card_chimes_and_pulses_once)
{
    const card_poll_t foreign = card(5u, 0u);

    setup(true);
    feed(&foreign, 2000u);
    CHECK_EQ(1, total(APP_CMD_AUDIO_CHIME, NULL));
    CHECK_EQ(1, total(APP_CMD_LAMP_PULSE, NULL));
    CHECK_EQ(0, total(APP_CMD_AUDIO_PLAY, NULL));
}

TEST(two_cards_chime_once)
{
    setup(true);
    feed(&COLLISION, 2000u);
    CHECK_EQ(1, total(APP_CMD_AUDIO_CHIME, NULL));
}

/* ---- F10: lamp button --------------------------------------------------- */

TEST(button_without_card_toggles_plain_white)
{
    app_cmd_t cmd;

    setup(true);
    clear_log();
    press(APP_IN_LAMP_PRESS, 0);
    CHECK_EQ(1, total(APP_CMD_LAMP_TINT, &cmd));
    CHECK_EQ(0, cmd.level_pm);                  /* no colour */
    CHECK_EQ(1, total(APP_CMD_LAMP_FADE, &cmd));
    CHECK_EQ(APP_LEVEL_FULL_PM, cmd.level_pm);
    clear_log();
    press(APP_IN_LAMP_PRESS, 0);
    CHECK_EQ(1, total(APP_CMD_LAMP_FADE, &cmd));
    CHECK_EQ(0, cmd.level_pm);
}

TEST(button_during_story_turns_lamp_off_story_continues)
{
    const card_poll_t c = card(1u, 7u);
    app_cmd_t         cmd;

    setup(true);
    feed(&c, 600u);
    clear_log();
    press(APP_IN_LAMP_PRESS, 0);
    feed(&c, 3000u);
    CHECK_EQ(1, total(APP_CMD_LAMP_FADE, &cmd));
    CHECK_EQ(0, cmd.level_pm);
    CHECK_EQ(0, total(APP_CMD_AUDIO_PAUSE, NULL));
    CHECK_EQ(0, total(APP_CMD_AUDIO_STOP, NULL));
}

TEST(lamp_switched_off_stays_off_at_story_end)
{
    const card_poll_t c = card(1u, 7u);

    setup(true);
    feed(&c, 600u);
    press(APP_IN_LAMP_PRESS, 0);
    clear_log();
    collect(app_logic_story_end(&s_app, false, s_now));
    CHECK_EQ(0, total(APP_CMD_LAMP_FADE, NULL));
    CHECK_EQ(LAMP_MODE_OFF, s_app.lamp);
}

TEST(lamp_switched_off_comes_back_when_card_is_placed_again)
{
    const card_poll_t c = card(1u, 7u);
    app_cmd_t         cmd;

    setup(true);
    feed(&c, 600u);
    press(APP_IN_LAMP_PRESS, 0);
    feed(&NO_CARD, 4000u);                      /* paused */
    clear_log();
    feed(&c, 600u);                             /* resume */
    CHECK_EQ(1, total(APP_CMD_LAMP_FADE, &cmd));
    CHECK_EQ(APP_LEVEL_FULL_PM, cmd.level_pm);
}

TEST(manual_lamp_then_card_resets_to_story_lamp)
{
    const card_poll_t c = card(1u, 7u);
    app_cmd_t         cmd;

    setup(true);
    press(APP_IN_LAMP_PRESS, 0);                /* manual on */
    clear_log();
    feed(&c, 600u);
    CHECK_EQ(1, total(APP_CMD_LAMP_TINT, &cmd));
    CHECK_EQ(42, cmd.arg);
    CHECK_EQ(LAMP_MODE_STORY, s_app.lamp);
}

TEST(button_during_glow_turns_off)
{
    const card_poll_t c = card(1u, 7u);
    app_cmd_t         cmd;

    setup(true);
    feed(&c, 600u);
    collect(app_logic_story_end(&s_app, false, s_now));
    clear_log();
    press(APP_IN_LAMP_PRESS, 0);
    CHECK_EQ(1, total(APP_CMD_LAMP_FADE, &cmd));
    CHECK_EQ(0, cmd.level_pm);
    CHECK_EQ(LAMP_MODE_OFF, s_app.lamp);
}

/* ---- F11 / F14: volume and mute ----------------------------------------- */

TEST(volume_steps_are_clamped)
{
    app_cmd_t cmd;

    setup(true);
    clear_log();
    press(APP_IN_ENC_STEP, 100);
    CHECK_EQ(1, total(APP_CMD_AUDIO_VOLUME, &cmd));
    CHECK_EQ(APP_VOLUME_MAX, cmd.arg);
    clear_log();
    press(APP_IN_ENC_STEP, 1);                  /* already at max */
    CHECK_EQ(0, total(APP_CMD_AUDIO_VOLUME, NULL));
    press(APP_IN_ENC_STEP, -100);
    CHECK_EQ(1, total(APP_CMD_AUDIO_VOLUME, &cmd));
    CHECK_EQ(0, cmd.arg);
}

TEST(press_mutes_turn_unmutes)
{
    app_cmd_t cmd;

    setup(true);
    s_now += 5000u;
    clear_log();
    press(APP_IN_ENC_PRESS, 0);
    CHECK_EQ(1, total(APP_CMD_AUDIO_MUTE, &cmd));
    CHECK_EQ(1, cmd.arg);
    clear_log();
    s_now += 2000u;
    press(APP_IN_ENC_STEP, 1);
    CHECK_EQ(1, total(APP_CMD_AUDIO_MUTE, &cmd));
    CHECK_EQ(0, cmd.arg);
}

TEST(knob_wobble_right_after_press_keeps_mute)
{
    app_cmd_t cmd;

    setup(true);
    s_now += 5000u;
    press(APP_IN_ENC_PRESS, 0);
    clear_log();
    s_now += 200u;
    press(APP_IN_ENC_STEP, 1);
    CHECK_EQ(0, total(APP_CMD_AUDIO_MUTE, NULL));
    s_now += APP_MUTE_GUARD_MS;
    press(APP_IN_ENC_STEP, 1);
    CHECK_EQ(1, total(APP_CMD_AUDIO_MUTE, &cmd));
    CHECK_EQ(0, cmd.arg);
}

TEST(card_swap_waits_only_briefly)
{
    const card_poll_t a = card(1u, 7u);
    const card_poll_t b = card(9u, 8u);

    setup(true);
    feed(&a, 600u + APP_STORY_DELAY_MS);
    clear_log();
    feed(&NO_CARD, 300u);
    feed(&b, 600u + APP_SWITCH_DELAY_MS);
    CHECK_EQ(1, total(APP_CMD_AUDIO_STOP, NULL));
    CHECK_EQ(1, total(APP_CMD_AUDIO_PLAY, NULL));
}

TEST(card_placement_unmutes)
{
    const card_poll_t c = card(1u, 7u);
    app_cmd_t         cmd;

    setup(true);
    press(APP_IN_ENC_PRESS, 0);
    clear_log();
    feed(&c, 600u);
    CHECK_EQ(1, total(APP_CMD_AUDIO_MUTE, &cmd));
    CHECK_EQ(0, cmd.arg);
}

/* ---- Errors -------------------------------------------------------------- */

TEST(missing_story_chimes_and_lamp_goes_off)
{
    const card_poll_t c = card(1u, 77u);
    app_cmd_t         cmd;

    setup(true);
    feed(&c, 600u);
    clear_log();
    collect(app_logic_story_end(&s_app, true, s_now));
    CHECK_EQ(1, total(APP_CMD_AUDIO_CHIME, NULL));
    CHECK_EQ(1, total(APP_CMD_LAMP_FADE, &cmd));
    CHECK_EQ(0, cmd.level_pm);
    clear_log();
    feed(&c, 3000u);                            /* card still on: no retry loop */
    CHECK_EQ(0, total(APP_CMD_AUDIO_PLAY, NULL));
}

TEST(sd_fault_pulses_at_boot_and_never_plays)
{
    const card_poll_t c = card(1u, 7u);

    setup(false);
    feed(&NO_CARD, 5000u);
    CHECK_EQ(APP_FAULT_PULSES, total(APP_CMD_LAMP_PULSE, NULL));
    clear_log();
    feed(&c, 600u);
    CHECK_EQ(0, total(APP_CMD_AUDIO_PLAY, NULL));
    CHECK_EQ(1, total(APP_CMD_LAMP_PULSE, NULL));
}

static void run_suite(void)
{
    RUN(card_starts_story_warms_lamp_with_card_colour);
    RUN(short_lift_breathes_and_restores_without_pause);
    RUN(long_lift_pauses_lamp_stays_full_and_resume_works);
    RUN(other_card_switches_story);
    RUN(story_waits_for_the_light);
    RUN(paused_before_start_starts_on_return);
    RUN(other_card_during_wait_replaces_story);
    RUN(story_end_glows_holds_then_fades_off);
    RUN(card_left_on_after_end_does_not_restart);
    RUN(pause_timeout_goes_to_glow);
    RUN(unknown_card_chimes_and_pulses_once);
    RUN(two_cards_chime_once);
    RUN(button_without_card_toggles_plain_white);
    RUN(button_during_story_turns_lamp_off_story_continues);
    RUN(lamp_switched_off_stays_off_at_story_end);
    RUN(lamp_switched_off_comes_back_when_card_is_placed_again);
    RUN(manual_lamp_then_card_resets_to_story_lamp);
    RUN(button_during_glow_turns_off);
    RUN(volume_steps_are_clamped);
    RUN(press_mutes_turn_unmutes);
    RUN(knob_wobble_right_after_press_keeps_mute);
    RUN(card_swap_waits_only_briefly);
    RUN(card_placement_unmutes);
    RUN(missing_story_chimes_and_lamp_goes_off);
    RUN(sd_fault_pulses_at_boot_and_never_plays);
}
