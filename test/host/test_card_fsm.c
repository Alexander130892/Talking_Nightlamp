/*
 * Executable specification for card_fsm (requirements F1-F9, ADR 0004).
 * Polls are simulated every POLL_MS with the default configuration:
 * present_hits = 2, grace = 3000 ms, pause timeout = 10 min.
 */

#include <string.h>
#include "test.h"
#include "card_fsm.h"

#define POLL_MS (150u)

static card_fsm_t s_fsm;
static uint32_t   s_now;

static card_poll_t poll_none(void)
{
    card_poll_t p;

    (void)memset(&p, 0, sizeof(p));
    return p;
}

static card_poll_t poll_tag(uint8_t uid0, bool b_valid, uint8_t story)
{
    card_poll_t p = poll_none();

    p.b_present = true;
    p.b_valid   = b_valid;
    p.uid[0]    = uid0;
    p.uid[1]    = 0xA5u;
    p.uid[2]    = 0x5Au;
    p.uid[3]    = 0x11u;
    p.uid_len   = 4u;
    p.story_id  = story;
    p.hue       = (uint8_t)(story * 10u);
    return p;
}

static card_poll_t card_a(void) { return poll_tag(0x01u, true, 7u); }
static card_poll_t card_b(void) { return poll_tag(0x02u, true, 9u); }
static card_poll_t foreign(void) { return poll_tag(0x03u, false, 0u); }

static void setup(uint32_t t0)
{
    const card_fsm_cfg_t cfg = CARD_FSM_CFG_DEFAULT;

    card_fsm_init(&s_fsm, &cfg);
    s_now = t0;
}

/* Advance one poll period and feed p. */
static card_action_t step(card_poll_t p)
{
    s_now += POLL_MS;
    return card_fsm_step(&s_fsm, &p, s_now).action;
}

/* Feed p n times; return how often `want` was produced. */
static int count(card_poll_t p, int n, card_action_t want)
{
    int hits = 0;

    for (int i = 0; i < n; i++)
    {
        if (want == step(p))
        {
            hits++;
        }
    }
    return hits;
}

/* Place card_a and return once it is PLAYING. */
static void start_playing(void)
{
    (void)step(card_a());
    (void)step(card_a());
}

/* ---- F1: start ---------------------------------------------------- */

TEST(idle_without_card_does_nothing)
{
    setup(0u);
    CHECK_EQ(0, 50 - count(poll_none(), 50, CARD_ACT_NONE));
    CHECK_EQ(CARD_FSM_IDLE, card_fsm_state(&s_fsm));
}

TEST(single_hit_does_not_start)
{
    setup(0u);
    CHECK_EQ(CARD_ACT_NONE, step(card_a()));
    CHECK_EQ(CARD_FSM_IDLE, card_fsm_state(&s_fsm));
}

TEST(stable_card_starts_its_story)
{
    card_poll_t   p = card_a();
    card_output_t out;

    setup(0u);
    (void)step(p);
    s_now += POLL_MS;
    out = card_fsm_step(&s_fsm, &p, s_now);
    CHECK_EQ(CARD_ACT_PLAY, out.action);
    CHECK_EQ(7, out.story_id);
    CHECK_EQ(70, out.hue);
    CHECK_EQ(CARD_FSM_PLAYING, card_fsm_state(&s_fsm));
}

TEST(card_left_on_reader_plays_once)
{
    setup(0u);
    CHECK_EQ(1, count(card_a(), 100, CARD_ACT_PLAY));
}

/* ---- F2-F4: lift, grace, pause ------------------------------------ */

TEST(lift_starts_grace_and_signals_lamp)
{
    setup(0u);
    start_playing();
    CHECK_EQ(CARD_ACT_LAMP_LOST, step(poll_none()));
    CHECK_EQ(CARD_FSM_GRACE, card_fsm_state(&s_fsm));
}

TEST(return_within_grace_restores_without_interruption)
{
    setup(0u);
    start_playing();
    (void)step(poll_none());
    (void)step(poll_none());
    CHECK_EQ(CARD_ACT_LAMP_RESTORE, step(card_a()));
    CHECK_EQ(CARD_FSM_PLAYING, card_fsm_state(&s_fsm));
}

TEST(grace_expiry_pauses_once_after_3s)
{
    uint32_t t_lost;
    int      pauses = 0;

    setup(0u);
    start_playing();
    (void)step(poll_none());
    t_lost = s_now;
    while (((s_now + POLL_MS) - t_lost) < 3000u) /* next poll still < 3 s */
    {
        CHECK(CARD_ACT_PAUSE != step(poll_none()));
    }
    for (int i = 0; i < 20; i++)
    {
        if (CARD_ACT_PAUSE == step(poll_none()))
        {
            pauses++;
            CHECK((s_now - t_lost) >= 3000u);
        }
    }
    CHECK_EQ(1, pauses);
    CHECK_EQ(CARD_FSM_PAUSED, card_fsm_state(&s_fsm));
}

/* ---- F5, F6: resume and timeout ----------------------------------- */

static void pause_card_a(void)
{
    start_playing();
    (void)count(poll_none(), 30, CARD_ACT_PAUSE); /* 4.5 s */
}

TEST(same_card_resumes_after_pause)
{
    setup(0u);
    pause_card_a();
    CHECK_EQ(CARD_FSM_PAUSED, card_fsm_state(&s_fsm));
    CHECK_EQ(1, count(card_a(), 5, CARD_ACT_RESUME));
    CHECK_EQ(CARD_FSM_PLAYING, card_fsm_state(&s_fsm));
}

TEST(pause_timeout_returns_to_idle_and_restarts)
{
    card_poll_t   p = card_a();
    card_output_t out;

    setup(0u);
    pause_card_a();
    s_now += (10u * 60u * 1000u);
    CHECK_EQ(CARD_ACT_STOP, step(poll_none()));
    CHECK_EQ(CARD_FSM_IDLE, card_fsm_state(&s_fsm));

    (void)step(p);
    s_now += POLL_MS;
    out = card_fsm_step(&s_fsm, &p, s_now);
    CHECK_EQ(CARD_ACT_PLAY, out.action);
    CHECK_EQ(7, out.story_id);
}

/* ---- F7: switching cards ------------------------------------------ */

TEST(other_card_during_grace_switches)
{
    setup(0u);
    start_playing();
    (void)step(poll_none());
    CHECK_EQ(1, count(card_b(), 3, CARD_ACT_PLAY));
    CHECK_EQ(CARD_FSM_PLAYING, card_fsm_state(&s_fsm));
}

TEST(other_card_while_paused_starts_new_story)
{
    card_poll_t   p = card_b();
    card_output_t out;

    setup(0u);
    pause_card_a();
    (void)step(p);
    s_now += POLL_MS;
    out = card_fsm_step(&s_fsm, &p, s_now);
    CHECK_EQ(CARD_ACT_PLAY, out.action);
    CHECK_EQ(9, out.story_id);
}

/* ---- F8: story end ------------------------------------------------ */

TEST(story_end_stops_and_does_not_loop)
{
    setup(0u);
    start_playing();
    CHECK_EQ(CARD_ACT_STOP, card_fsm_story_finished(&s_fsm, s_now).action);
    CHECK_EQ(CARD_FSM_IDLE, card_fsm_state(&s_fsm));
    CHECK_EQ(0, count(card_a(), 50, CARD_ACT_PLAY));
}

TEST(story_end_then_replace_card_plays_again)
{
    setup(0u);
    start_playing();
    (void)card_fsm_story_finished(&s_fsm, s_now);
    (void)count(poll_none(), 3, CARD_ACT_NONE);
    CHECK_EQ(1, count(card_a(), 5, CARD_ACT_PLAY));
}

/* ---- F9: unknown tags --------------------------------------------- */

TEST(unknown_tag_reported_once_per_placement)
{
    setup(0u);
    CHECK_EQ(1, count(foreign(), 30, CARD_ACT_UNKNOWN));
    CHECK_EQ(CARD_FSM_IDLE, card_fsm_state(&s_fsm));
    (void)count(poll_none(), 3, CARD_ACT_NONE);
    CHECK_EQ(1, count(foreign(), 30, CARD_ACT_UNKNOWN));
}

TEST(unknown_tag_during_grace_keeps_grace_timer)
{
    setup(0u);
    start_playing();
    (void)step(poll_none());
    CHECK_EQ(1, count(foreign(), 30, CARD_ACT_UNKNOWN));
    CHECK_EQ(CARD_FSM_PAUSED, card_fsm_state(&s_fsm));
}

/* ---- F13: two cards at once (collision) --------------------------- */

static card_poll_t collision(void)
{
    card_poll_t p = poll_none();

    p.b_present = true;  /* something answered WUPA            */
    p.b_valid   = false; /* but no single card could be selected */
    return p;            /* uid_len 0                           */
}

TEST(two_cards_reported_as_unknown_once)
{
    setup(0u);
    CHECK_EQ(1, count(collision(), 30, CARD_ACT_UNKNOWN));
    CHECK_EQ(CARD_FSM_IDLE, card_fsm_state(&s_fsm));
}

TEST(second_card_on_playing_card_chimes_then_pauses)
{
    setup(0u);
    start_playing();
    CHECK_EQ(CARD_ACT_LAMP_LOST, step(collision()));
    CHECK_EQ(1, count(collision(), 30, CARD_ACT_UNKNOWN));
    CHECK_EQ(CARD_FSM_PAUSED, card_fsm_state(&s_fsm));
}

TEST(second_card_removed_within_grace_restores)
{
    setup(0u);
    start_playing();
    (void)step(collision());
    (void)step(collision());
    CHECK_EQ(CARD_ACT_LAMP_RESTORE, step(card_a()));
    CHECK_EQ(CARD_FSM_PLAYING, card_fsm_state(&s_fsm));
}

/* ---- Robustness --------------------------------------------------- */

TEST(grace_works_across_timer_wrap)
{
    setup(0xFFFFFF00u);
    start_playing();
    CHECK_EQ(CARD_ACT_LAMP_LOST, step(poll_none()));
    CHECK_EQ(1, count(poll_none(), 30, CARD_ACT_PAUSE));
}

static void run_suite(void)
{
    RUN(idle_without_card_does_nothing);
    RUN(single_hit_does_not_start);
    RUN(stable_card_starts_its_story);
    RUN(card_left_on_reader_plays_once);
    RUN(lift_starts_grace_and_signals_lamp);
    RUN(return_within_grace_restores_without_interruption);
    RUN(grace_expiry_pauses_once_after_3s);
    RUN(same_card_resumes_after_pause);
    RUN(pause_timeout_returns_to_idle_and_restarts);
    RUN(other_card_during_grace_switches);
    RUN(other_card_while_paused_starts_new_story);
    RUN(story_end_stops_and_does_not_loop);
    RUN(story_end_then_replace_card_plays_again);
    RUN(unknown_tag_reported_once_per_placement);
    RUN(unknown_tag_during_grace_keeps_grace_timer);
    RUN(two_cards_reported_as_unknown_once);
    RUN(second_card_on_playing_card_chimes_then_pauses);
    RUN(second_card_removed_within_grace_restores);
    RUN(grace_works_across_timer_wrap);
}
