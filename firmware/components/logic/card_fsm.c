/**
 * @file  card_fsm.c
 * @brief Card presence state machine.
 *
 * Structure (docs/firmware/architecture.md):
 *   1. debounce()  - track consecutive polls of the same tag
 *   2. classify()  - turn poll + debounce into ONE event
 *   3. switch on state - transition table (see card_fsm.h, requirements)
 *
 * Specification: test/host/test_card_fsm.c (make -C test/host run-card_fsm)
 */

#include <string.h>
#include "card_fsm.h"

/** Events derived from one poll. */
typedef enum
{
    EVT_NO_TAG = 0,  /* nothing answered */
    EVT_PENDING,     /* a tag answered but is not stable yet, or an unknown
                        tag that was already reported this placement */
    EVT_CUR,         /* the current card (one hit is enough); check
                        is_stable() where a debounced hit is required */
    EVT_OTHER_VALID, /* stable valid card that is not the current card */
    EVT_UNKNOWN      /* stable invalid tag / collision, not yet reported */
} card_evt_t;

/* ---- Helpers ------------------------------------------------------- */

static bool uid_equal(const uint8_t *p_a, uint8_t len_a,
                      const uint8_t *p_b, uint8_t len_b)
{
    return (len_a == len_b) && (0 == memcmp(p_a, p_b, len_a));
}

static bool is_stable(const card_fsm_t *p_fsm)
{
    return (p_fsm->cand_hits >= p_fsm->cfg.present_hits);
}

static bool elapsed(uint32_t now_ms, uint32_t t_mark_ms, uint32_t dur_ms)
{
    return ((uint32_t)(now_ms - t_mark_ms) >= dur_ms); /* wrap-safe */
}

static card_output_t act(card_action_t action)
{
    card_output_t out = {action, 0u, 0u};

    return out;
}

/** Make the polled card the current card and start its story. */
static card_output_t start_play(card_fsm_t *p_fsm, const card_poll_t *p_poll)
{
    card_output_t out = {CARD_ACT_PLAY, p_poll->story_id, p_poll->hue};

    (void)memcpy(p_fsm->cur_uid, p_poll->uid, p_poll->uid_len);
    p_fsm->cur_uid_len = p_poll->uid_len;
    p_fsm->state       = CARD_FSM_PLAYING;
    return out;
}

/** Forget the current card (story stopped). */
static void forget_current(card_fsm_t *p_fsm)
{
    p_fsm->cur_uid_len = 0u;
    p_fsm->state       = CARD_FSM_IDLE;
}

/** Report an unknown tag; classify() makes sure it's once per placement. */
static card_output_t report_unknown(card_fsm_t *p_fsm)
{
    p_fsm->b_unknown_reported = true;
    return act(CARD_ACT_UNKNOWN);
}

/* ---- 1. Debounce --------------------------------------------------- */

static void debounce(card_fsm_t *p_fsm, const card_poll_t *p_poll)
{
    if (!p_poll->b_present)
    {
        /* Tag gone: a new placement starts with the next tag. */
        p_fsm->cand_uid_len       = 0u;
        p_fsm->cand_hits          = 0u;
        p_fsm->b_unknown_reported = false;
        p_fsm->b_wait_removal     = false;
    }
    else if ((p_fsm->cand_hits > 0u) &&
             uid_equal(p_poll->uid, p_poll->uid_len,
                       p_fsm->cand_uid, p_fsm->cand_uid_len))
    {
        if (p_fsm->cand_hits < UINT8_MAX)
        {
            p_fsm->cand_hits++;
        }
    }
    else
    {
        /* Different tag than last poll: new candidate. */
        (void)memcpy(p_fsm->cand_uid, p_poll->uid, p_poll->uid_len);
        p_fsm->cand_uid_len       = p_poll->uid_len;
        p_fsm->cand_hits          = 1u;
        p_fsm->b_unknown_reported = false;
    }
}

/* ---- 2. Classify --------------------------------------------------- */

static card_evt_t classify(const card_fsm_t *p_fsm, const card_poll_t *p_poll)
{
    card_evt_t evt = EVT_PENDING;

    if (!p_poll->b_present)
    {
        evt = EVT_NO_TAG;
    }
    else if (p_poll->b_valid && (p_fsm->cur_uid_len > 0u) &&
             uid_equal(p_poll->uid, p_poll->uid_len,
                       p_fsm->cur_uid, p_fsm->cur_uid_len))
    {
        evt = EVT_CUR;
    }
    else if (!is_stable(p_fsm))
    {
        evt = EVT_PENDING;
    }
    else if (p_poll->b_valid)
    {
        evt = EVT_OTHER_VALID;
    }
    else if (!p_fsm->b_unknown_reported)
    {
        evt = EVT_UNKNOWN;
    }
    else
    {
        evt = EVT_PENDING;
    }

    return evt;
}

/* ---- Public API ---------------------------------------------------- */

void card_fsm_init(card_fsm_t *p_fsm, const card_fsm_cfg_t *p_cfg)
{
    (void)memset(p_fsm, 0, sizeof(*p_fsm));
    p_fsm->cfg   = *p_cfg;
    p_fsm->state = CARD_FSM_IDLE;
}

card_output_t card_fsm_step(card_fsm_t *p_fsm, const card_poll_t *p_poll,
                            uint32_t now_ms)
{
    card_output_t out = act(CARD_ACT_NONE);
    card_evt_t    evt;

    debounce(p_fsm, p_poll);
    evt = classify(p_fsm, p_poll);

    switch (p_fsm->state)
    {
        case CARD_FSM_IDLE:
            if ((EVT_OTHER_VALID == evt) && !p_fsm->b_wait_removal)
            {
                out = start_play(p_fsm, p_poll);
            }
            else if (EVT_UNKNOWN == evt)
            {
                out = report_unknown(p_fsm);
            }
            else
            {
                /* nothing to do */
            }
            break;

        case CARD_FSM_PLAYING:
            if (EVT_CUR != evt)
            {
                p_fsm->state     = CARD_FSM_GRACE;
                p_fsm->t_mark_ms = now_ms;
                out              = act(CARD_ACT_LAMP_LOST);
            }
            break;

        case CARD_FSM_GRACE:
            if (EVT_CUR == evt)
            {
                p_fsm->state = CARD_FSM_PLAYING;
                out          = act(CARD_ACT_LAMP_RESTORE);
            }
            else if (EVT_OTHER_VALID == evt)
            {
                out = start_play(p_fsm, p_poll);
            }
            else if (EVT_UNKNOWN == evt)
            {
                out = report_unknown(p_fsm);
            }
            else if (elapsed(now_ms, p_fsm->t_mark_ms, p_fsm->cfg.grace_ms))
            {
                p_fsm->state     = CARD_FSM_PAUSED;
                p_fsm->t_mark_ms = now_ms;
                out              = act(CARD_ACT_PAUSE);
            }
            else
            {
                /* still within grace */
            }
            break;

        case CARD_FSM_PAUSED:
            if (elapsed(now_ms, p_fsm->t_mark_ms,
                        p_fsm->cfg.pause_timeout_ms))
            {
                forget_current(p_fsm);
                out = act(CARD_ACT_STOP);
            }
            else if ((EVT_CUR == evt) && is_stable(p_fsm))
            {
                p_fsm->state = CARD_FSM_PLAYING;
                out          = act(CARD_ACT_RESUME);
            }
            else if (EVT_OTHER_VALID == evt)
            {
                out = start_play(p_fsm, p_poll);
            }
            else if (EVT_UNKNOWN == evt)
            {
                out = report_unknown(p_fsm);
            }
            else
            {
                /* waiting */
            }
            break;

        default:
            forget_current(p_fsm);
            break;
    }

    return out;
}

card_output_t card_fsm_story_finished(card_fsm_t *p_fsm, uint32_t now_ms)
{
    card_output_t out = act(CARD_ACT_NONE);

    (void)now_ms;

    if ((CARD_FSM_PLAYING == p_fsm->state) || (CARD_FSM_GRACE == p_fsm->state))
    {
        forget_current(p_fsm);
        p_fsm->b_wait_removal = true;
        out                   = act(CARD_ACT_STOP);
    }

    return out;
}

card_fsm_state_t card_fsm_state(const card_fsm_t *p_fsm)
{
    return p_fsm->state;
}