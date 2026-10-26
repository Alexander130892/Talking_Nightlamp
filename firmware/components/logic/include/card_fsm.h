/**
 * @file  card_fsm.h
 * @brief Card presence state machine: decides play / pause / resume / switch.
 *
 * Pure logic: fed one poll result per RFID period plus a millisecond clock.
 * Behaviour: docs/firmware/architecture.md and requirements F1-F9.
 * The specification is executable: test/host/test_card_fsm.c.
 *
 * Contract
 *  - Time is a free-running uint32_t ms counter. Compare times only by
 *    subtraction ((now - t) >= d) so a counter wrap is harmless.
 *  - A valid card is "present" after cfg.present_hits consecutive polls
 *    with the same UID; only then does PLAY, RESUME or UNKNOWN fire.
 *  - Exception: the current card returning during GRACE restores on its
 *    first hit (LAMP_RESTORE), so a brief lift is never audible.
 *  - The grace timer keeps running while an unknown tag lies on the reader.
 *  - Two cards at once (collision) are fed as b_present = true,
 *    b_valid = false, uid_len = 0 and handled as an unknown tag (F13).
 *  - Each card_fsm_step() returns at most one action.
 *  - An invalid/unknown tag produces CARD_ACT_UNKNOWN once per placement
 *    (not every poll) and never changes the state.
 *  - After a story finishes (STOP), the same card must be removed before
 *    it can start again: a card left on the reader doesn't loop the story.
 */

#ifndef CARD_FSM_H
#define CARD_FSM_H

#include <stdbool.h>
#include <stdint.h>

#define CARD_UID_MAX_LEN (10u)

typedef enum
{
    CARD_FSM_IDLE = 0,
    CARD_FSM_PLAYING,
    CARD_FSM_GRACE,
    CARD_FSM_PAUSED
} card_fsm_state_t;

typedef enum
{
    CARD_ACT_NONE = 0,
    CARD_ACT_PLAY,        /* start story out.story_id from the beginning */
    CARD_ACT_RESUME,      /* continue the paused story */
    CARD_ACT_PAUSE,       /* grace period expired */
    CARD_ACT_STOP,        /* story finished or pause timed out: stop audio,
                             fade lamp to glow, then off */
    CARD_ACT_LAMP_LOST,   /* card lifted: start grace signal on the lamp */
    CARD_ACT_LAMP_RESTORE,/* card back within grace: end the signal */
    CARD_ACT_UNKNOWN      /* unrecognised tag: pulse + chime */
} card_action_t;

typedef struct
{
    uint8_t  present_hits;     /* consecutive hits before "present", >= 1 */
    uint32_t grace_ms;         /* audio keeps playing this long, F2 */
    uint32_t pause_timeout_ms; /* PAUSED -> IDLE after this long, F6 */
} card_fsm_cfg_t;

/* Defaults from the requirements. */
#define CARD_FSM_CFG_DEFAULT                                                  \
    {                                                                         \
        .present_hits = 2u, .grace_ms = 3000u,                                \
        .pause_timeout_ms = (10u * 60u * 1000u)                               \
    }

/** One RFID poll result. */
typedef struct
{
    bool    b_present;              /* any tag answered */
    bool    b_valid;                /* tag carried a valid story record */
    uint8_t uid[CARD_UID_MAX_LEN];
    uint8_t uid_len;
    uint8_t story_id;               /* valid only if b_valid */
    uint8_t hue;                    /* valid only if b_valid */
} card_poll_t;

typedef struct
{
    card_action_t action;
    uint8_t       story_id; /* set with PLAY */
    uint8_t       hue;      /* set with PLAY */
} card_output_t;

/** Internal state. Treat as opaque; fields are public only for sizing. */
typedef struct
{
    card_fsm_cfg_t   cfg;
    card_fsm_state_t state;
    /* Suggested fields for the implementation; change freely. */
    uint8_t  cur_uid[CARD_UID_MAX_LEN];
    uint8_t  cur_uid_len;
    uint8_t  cand_uid[CARD_UID_MAX_LEN];
    uint8_t  cand_uid_len;
    uint8_t  cand_hits;
    uint32_t t_mark_ms;
    bool     b_unknown_reported;
    bool     b_wait_removal;
} card_fsm_t;

void card_fsm_init(card_fsm_t *p_fsm, const card_fsm_cfg_t *p_cfg);

/** Feed one poll result. Returns the action the app must perform. */
card_output_t card_fsm_step(card_fsm_t *p_fsm, const card_poll_t *p_poll,
                            uint32_t now_ms);

/** The audio pipeline reached the end of the story. */
card_output_t card_fsm_story_finished(card_fsm_t *p_fsm, uint32_t now_ms);

card_fsm_state_t card_fsm_state(const card_fsm_t *p_fsm);

#endif /* CARD_FSM_H */
