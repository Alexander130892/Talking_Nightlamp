/**
 * @file  app_logic.c
 * @brief Lamp behaviour policy (see app_logic.h).
 */

#include "app_logic.h"

#include <string.h>

/* ---- Command list helpers ------------------------------------------ */

static void add(app_cmds_t *p_c, app_cmd_type_t type, uint8_t arg,
                uint16_t level_pm, uint32_t dur_ms)
{
    if (p_c->n < APP_MAX_CMDS)
    {
        p_c->cmd[p_c->n] = (app_cmd_t){ .type = type, .arg = arg,
                                        .level_pm = level_pm, .dur_ms = dur_ms };
        p_c->n++;
    }
}

static void lamp_fade(app_cmds_t *p_c, uint16_t level_pm, uint32_t dur_ms)
{
    add(p_c, APP_CMD_LAMP_FADE, 0u, level_pm, dur_ms);
}

static void unmute(app_logic_t *p_app, app_cmds_t *p_c)
{
    if (p_app->b_mute)
    {
        p_app->b_mute = false;
        add(p_c, APP_CMD_AUDIO_MUTE, 0u, 0u, 0u);
    }
}

static bool story_active(const app_logic_t *p_app)
{
    return CARD_FSM_IDLE != card_fsm_state(&p_app->fsm);
}

static bool lamp_lit(const app_logic_t *p_app)
{
    return (LAMP_MODE_OFF != p_app->lamp) &&
           !((LAMP_MODE_STORY == p_app->lamp) && p_app->b_user_off);
}

/* ---- Story start / end ---------------------------------------------- */

/** A card started or resumed a story: reset the lamp (policy). */
static void card_lamp(app_logic_t *p_app, app_cmds_t *p_c, bool b_new_hue, uint8_t hue)
{
    if (b_new_hue)
    {
        add(p_c, APP_CMD_LAMP_TINT, hue, APP_TINT_PM, 0u);
    }
    p_app->lamp       = LAMP_MODE_STORY;
    p_app->b_user_off = false;
    lamp_fade(p_c, APP_LEVEL_FULL_PM, b_new_hue ? APP_WARMUP_MS : APP_RESUME_MS);
}

static void story_stopped(app_logic_t *p_app, app_cmds_t *p_c, uint32_t now_ms)
{
    p_app->b_wait_start = false;
    p_app->b_started    = false;
    add(p_c, APP_CMD_AUDIO_STOP, 0u, 0u, 0u);
    add(p_c, APP_CMD_LAMP_BREATHE, 0u, 0u, 0u);
    if (lamp_lit(p_app))
    {
        p_app->lamp          = LAMP_MODE_GLOW;
        p_app->t_deadline_ms = now_ms + APP_GLOW_FADE_MS + APP_GLOW_HOLD_MS;
        lamp_fade(p_c, APP_LEVEL_GLOW_PM, APP_GLOW_FADE_MS);
    }
    else
    {
        p_app->lamp = LAMP_MODE_OFF;     /* she switched it off: stays off */
    }
    p_app->b_user_off = false;
}

/** Execute one card_fsm action. */
static void do_action(app_logic_t *p_app, app_cmds_t *p_c, card_output_t out,
                      uint32_t now_ms)
{
    switch (out.action)
    {
        case CARD_ACT_PLAY:
            if (p_app->b_fault)
            {
                add(p_c, APP_CMD_LAMP_PULSE, 0u, 0u, 0u);   /* no audio */
                break;
            }
            unmute(p_app, p_c);
            if (p_app->b_started)
            {
                add(p_c, APP_CMD_AUDIO_STOP, 0u, 0u, 0u);  /* previous story */
            }
            p_app->b_started    = false;
            p_app->b_wait_start = true;              /* the light goes first */
            p_app->story_id     = out.story_id;
            p_app->t_start_ms   = now_ms +
                (((LAMP_MODE_STORY == p_app->lamp) && lamp_lit(p_app)) ? APP_SWITCH_DELAY_MS
                                                                       : APP_STORY_DELAY_MS);
            card_lamp(p_app, p_c, true, out.hue);
            break;
        case CARD_ACT_RESUME:
            unmute(p_app, p_c);
            if (p_app->b_started)
            {
                add(p_c, APP_CMD_AUDIO_RESUME, 0u, 0u, 0u);
            }
            else
            {
                p_app->b_wait_start = false;         /* paused before it began */
                p_app->b_started    = true;
                add(p_c, APP_CMD_AUDIO_PLAY, p_app->story_id, 0u, 0u);
            }
            card_lamp(p_app, p_c, false, 0u);
            break;
        case CARD_ACT_PAUSE:
            p_app->b_wait_start = false;   /* not started: starts on resume */
            add(p_c, APP_CMD_AUDIO_PAUSE, 0u, 0u, 0u);
            add(p_c, APP_CMD_LAMP_BREATHE, 0u, 0u, 0u);    /* stays full */
            break;
        case CARD_ACT_STOP:
            story_stopped(p_app, p_c, now_ms);
            break;
        case CARD_ACT_LAMP_LOST:
            add(p_c, APP_CMD_LAMP_BREATHE, 1u, 0u, 0u);
            break;
        case CARD_ACT_LAMP_RESTORE:
            add(p_c, APP_CMD_LAMP_BREATHE, 0u, 0u, 0u);
            break;
        case CARD_ACT_UNKNOWN:
            if (!p_app->b_fault)
            {
                add(p_c, APP_CMD_AUDIO_CHIME, 0u, 0u, 0u);
            }
            add(p_c, APP_CMD_LAMP_PULSE, 0u, 0u, 0u);
            break;
        case CARD_ACT_NONE:
        default:
            break;
    }
}

/* ---- Public API ------------------------------------------------------ */

app_cmds_t app_logic_init(app_logic_t *p_app, bool b_sd_ok, uint32_t now_ms)
{
    const card_fsm_cfg_t cfg = CARD_FSM_CFG_DEFAULT;
    app_cmds_t           c   = { 0 };

    (void)memset(p_app, 0, sizeof(*p_app));
    card_fsm_init(&p_app->fsm, &cfg);
    p_app->lamp    = LAMP_MODE_OFF;
    p_app->volume  = APP_VOLUME_START;
    p_app->b_fault = !b_sd_ok;
    if (p_app->b_fault)
    {
        p_app->fault_pulses_left = APP_FAULT_PULSES;
        p_app->t_next_pulse_ms   = now_ms;
    }
    add(&c, APP_CMD_AUDIO_VOLUME, p_app->volume, 0u, 0u);
    return c;
}

app_cmds_t app_logic_poll(app_logic_t *p_app, const card_poll_t *p_poll,
                          uint32_t now_ms)
{
    app_cmds_t c = { 0 };

    do_action(p_app, &c, card_fsm_step(&p_app->fsm, p_poll, now_ms), now_ms);
    return c;
}

app_cmds_t app_logic_input(app_logic_t *p_app, app_input_t in, int8_t steps,
                           uint32_t now_ms)
{
    app_cmds_t c = { 0 };

    switch (in)
    {
        case APP_IN_LAMP_PRESS:
            if (lamp_lit(p_app))
            {
                lamp_fade(&c, 0u, APP_TOGGLE_MS);
                if (story_active(p_app))
                {
                    p_app->b_user_off = true;      /* story keeps playing */
                }
                else
                {
                    p_app->lamp = LAMP_MODE_OFF;
                }
            }
            else if (story_active(p_app))
            {
                p_app->lamp       = LAMP_MODE_STORY;
                p_app->b_user_off = false;
                lamp_fade(&c, APP_LEVEL_FULL_PM, APP_TOGGLE_MS);
            }
            else
            {
                p_app->lamp = LAMP_MODE_MANUAL;
                add(&c, APP_CMD_LAMP_TINT, 0u, 0u, 0u);   /* plain warm white */
                lamp_fade(&c, APP_LEVEL_FULL_PM, APP_TOGGLE_MS);
            }
            break;

        case APP_IN_ENC_PRESS:
            p_app->t_press_ms = now_ms;
            p_app->b_mute     = !p_app->b_mute;
            add(&c, APP_CMD_AUDIO_MUTE, p_app->b_mute ? 1u : 0u, 0u, 0u);
            break;

        case APP_IN_ENC_STEP:
        {
            int v = (int)p_app->volume + (int)steps;

            v = (v < 0) ? 0 : ((v > (int)APP_VOLUME_MAX) ? (int)APP_VOLUME_MAX : v);
            if ((now_ms - p_app->t_press_ms) >= APP_MUTE_GUARD_MS)
            {
                unmute(p_app, &c);        /* a deliberate turn, not wobble */
            }
            if ((uint8_t)v != p_app->volume)
            {
                p_app->volume = (uint8_t)v;
                add(&c, APP_CMD_AUDIO_VOLUME, p_app->volume, 0u, 0u);
            }
            break;
        }

        default:
            break;
    }
    return c;
}

app_cmds_t app_logic_story_end(app_logic_t *p_app, bool b_error, uint32_t now_ms)
{
    app_cmds_t          c   = { 0 };
    const card_output_t out = card_fsm_story_finished(&p_app->fsm, now_ms);

    if (!b_error)
    {
        do_action(p_app, &c, out, now_ms);       /* STOP: glow sequence */
    }
    else
    {
        add(&c, APP_CMD_AUDIO_CHIME, 0u, 0u, 0u);
        add(&c, APP_CMD_LAMP_PULSE, 0u, 0u, 0u);
        add(&c, APP_CMD_LAMP_BREATHE, 0u, 0u, 0u);
        lamp_fade(&c, 0u, APP_ERROR_FADE_MS);
        p_app->b_wait_start = false;
        p_app->b_started    = false;
        p_app->lamp       = LAMP_MODE_OFF;
        p_app->b_user_off = false;
    }
    return c;
}

app_cmds_t app_logic_tick(app_logic_t *p_app, uint32_t now_ms)
{
    app_cmds_t c = { 0 };

    if (p_app->b_wait_start && ((int32_t)(now_ms - p_app->t_start_ms) >= 0))
    {
        p_app->b_wait_start = false;
        p_app->b_started    = true;
        add(&c, APP_CMD_AUDIO_PLAY, p_app->story_id, 0u, 0u);
    }

    if ((p_app->fault_pulses_left > 0u) &&
        ((int32_t)(now_ms - p_app->t_next_pulse_ms) >= 0))
    {
        add(&c, APP_CMD_LAMP_PULSE, 0u, 0u, 0u);
        p_app->fault_pulses_left--;
        p_app->t_next_pulse_ms = now_ms + APP_FAULT_PULSE_MS;
    }

    if (((LAMP_MODE_GLOW == p_app->lamp) || (LAMP_MODE_FADING_OFF == p_app->lamp)) &&
        ((int32_t)(now_ms - p_app->t_deadline_ms) >= 0))
    {
        if (LAMP_MODE_GLOW == p_app->lamp)
        {
            p_app->lamp          = LAMP_MODE_FADING_OFF;
            p_app->t_deadline_ms = now_ms + APP_OFF_FADE_MS;
            lamp_fade(&c, 0u, APP_OFF_FADE_MS);
        }
        else
        {
            p_app->lamp = LAMP_MODE_OFF;
            add(&c, APP_CMD_LAMP_TINT, 0u, 0u, 0u);
        }
    }
    return c;
}
