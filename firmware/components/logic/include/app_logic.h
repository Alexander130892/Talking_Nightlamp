/**
 * @file  app_logic.h
 * @brief The lamp's behaviour: events in, audio and lamp commands out.
 *
 * Pure logic, host-tested (test/host/test_app_logic.c): the requirements
 * F1-F15 as code. app.c only feeds events and executes the commands.
 * Wraps card_fsm and adds the policy around it.
 *
 * Policy (decided in step 17)
 *  - A card that starts or resumes a story (PLAY, RESUME) resets the lamp:
 *    full brightness, card colour, any manual on/off is forgotten (F1, F15).
 *    A new story warms up over APP_WARMUP_MS, a resume over APP_RESUME_MS.
 *  - The light sets the mood first: a new story's audio starts
 *    APP_STORY_DELAY_MS after the card is recognised. If the card is lifted
 *    long enough to pause before that, the story has not started yet; putting
 *    it back starts it at once. Another card replaces the waiting story.
 *  - Lamp button (F10) toggles the lamp at any time. During a story the
 *    story keeps playing; the choice holds until the next card placement.
 *    Without a story the lamp toggles plain warm white.
 *  - Card lifted: breathing (F2). Paused: breathing stops, lamp stays full.
 *  - Story end or pause timeout (F6, F8): fade to glow in APP_GLOW_FADE_MS,
 *    hold APP_GLOW_HOLD_MS, fade off in APP_OFF_FADE_MS. If she switched the
 *    lamp off, it stays off.
 *  - Unknown card or two cards (F9, F13): chime + one pulse.
 *  - Story folder missing: chime + pulse, lamp fades off, back to idle.
 *  - Encoder (F11): volume steps 0..APP_VOLUME_MAX; turning unmutes (F14).
 *    Encoder press toggles mute (F14); a card placement unmutes. Steps
 *    within APP_MUTE_GUARD_MS of a press do not unmute: pressing the knob
 *    can turn it a little.
 *  - When the lamp is already lit by a story (card swap), the next story
 *    waits only APP_SWITCH_DELAY_MS.
 *  - SD card fault: no audio at all; APP_FAULT_PULSES pulses at boot and one
 *    pulse per card placement, so the lamp still answers.
 */

#ifndef APP_LOGIC_H
#define APP_LOGIC_H

#include <stdbool.h>
#include <stdint.h>
#include "card_fsm.h"
#include "lamp_tint.h"

#define APP_VOLUME_MAX      (15u)
#define APP_VOLUME_START    (10u)
#define APP_LEVEL_FULL_PM   (1000u)
#define APP_LEVEL_GLOW_PM   (80u)          /* 12 LEDs at W = 1; tune in D2 */
#define APP_WARMUP_MS       (15000u)        /* new story: slow warm-up */
#define APP_RESUME_MS       (2000u)         /* resume: back to full */
#define APP_STORY_DELAY_MS  (8000u)        /* audio waits for the light */
#define APP_SWITCH_DELAY_MS (2000u)         /* lamp already lit: short wait */
#define APP_MUTE_GUARD_MS   (1000u)         /* knob wobble after a press */
#define APP_TOGGLE_MS       (500u)
#define APP_GLOW_FADE_MS    (30u * 1000u)
#define APP_GLOW_HOLD_MS    (15u * 60u * 1000u)
#define APP_OFF_FADE_MS     (2u * 60u * 1000u)
#define APP_ERROR_FADE_MS   (2000u)
#define APP_FAULT_PULSES    (3u)
#define APP_FAULT_PULSE_MS  (1000u)
#define APP_TINT_PM         (LAMP_TINT_DEFAULT_PM)   /* D5: 750 vs 1000 */
#define APP_MAX_CMDS        (8u)

typedef enum
{
    APP_CMD_AUDIO_PLAY,      /* arg = story id */
    APP_CMD_AUDIO_PAUSE,
    APP_CMD_AUDIO_RESUME,
    APP_CMD_AUDIO_STOP,
    APP_CMD_AUDIO_CHIME,
    APP_CMD_AUDIO_VOLUME,    /* arg = step */
    APP_CMD_AUDIO_MUTE,      /* arg = 0 / 1 */
    APP_CMD_LAMP_FADE,       /* level_pm, dur_ms */
    APP_CMD_LAMP_PULSE,
    APP_CMD_LAMP_BREATHE,    /* arg = 0 / 1 */
    APP_CMD_LAMP_TINT,       /* arg = hue, level_pm = strength */
} app_cmd_type_t;

typedef struct
{
    app_cmd_type_t type;
    uint8_t        arg;
    uint16_t       level_pm;
    uint32_t       dur_ms;
} app_cmd_t;

typedef struct
{
    uint8_t   n;
    app_cmd_t cmd[APP_MAX_CMDS];
} app_cmds_t;

typedef enum
{
    APP_IN_LAMP_PRESS,
    APP_IN_ENC_PRESS,
    APP_IN_ENC_STEP,
} app_input_t;

typedef enum
{
    LAMP_MODE_OFF,
    LAMP_MODE_MANUAL,       /* switched on with the button, no story */
    LAMP_MODE_STORY,        /* a story is playing / in grace / paused */
    LAMP_MODE_GLOW,         /* after the story: fading to and holding glow */
    LAMP_MODE_FADING_OFF,
} app_lamp_mode_t;

typedef struct
{
    card_fsm_t      fsm;
    app_lamp_mode_t lamp;
    bool            b_user_off;     /* she switched the story lamp off */
    uint32_t        t_deadline_ms;  /* GLOW / FADING_OFF phase end */
    uint8_t         volume;
    bool            b_mute;
    bool            b_fault;        /* no SD card: no audio */
    bool            b_wait_start;   /* story chosen, audio not started yet */
    bool            b_started;      /* audio driver has the story */
    uint8_t         story_id;
    uint32_t        t_start_ms;
    uint32_t        t_press_ms;     /* last encoder press */
    uint8_t         fault_pulses_left;
    uint32_t        t_next_pulse_ms;
} app_logic_t;

/** Start state. b_sd_ok = false selects the fault behaviour. */
app_cmds_t app_logic_init(app_logic_t *p_app, bool b_sd_ok, uint32_t now_ms);

/** One RFID poll (every RFID_PERIOD_MS). */
app_cmds_t app_logic_poll(app_logic_t *p_app, const card_poll_t *p_poll,
                          uint32_t now_ms);

/** A button or encoder event. steps only for APP_IN_ENC_STEP. */
app_cmds_t app_logic_input(app_logic_t *p_app, app_input_t in, int8_t steps,
                           uint32_t now_ms);

/** The audio driver finished the story (b_error: story could not play). */
app_cmds_t app_logic_story_end(app_logic_t *p_app, bool b_error, uint32_t now_ms);

/** Timers (glow phases, fault pulses). Call regularly, e.g. every 20 ms. */
app_cmds_t app_logic_tick(app_logic_t *p_app, uint32_t now_ms);

#endif /* APP_LOGIC_H */
