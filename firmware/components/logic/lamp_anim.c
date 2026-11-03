/**
 * @file  lamp_anim.c
 * @brief Fades, pulse and breathing (see lamp_anim.h for the contract).
 */

#include "lamp_anim.h"

#include <stddef.h>

#define PM (1000u)

static uint16_t clamp_pm(uint32_t v)
{
    return (uint16_t)((v > LAMP_ANIM_MAX_PM) ? LAMP_ANIM_MAX_PM : v);
}

/** Smooth bump over one period: 0 at both ends, PM in the middle.
 *  Triangle 0..PM..0, then smoothstep x*x*(3-2x) to round the corners. */
static uint32_t bump_pm(uint32_t elapsed_ms, uint32_t period_ms)
{
    const uint32_t half = period_ms / 2u;
    uint32_t       x;

    if ((0u == half) || (elapsed_ms >= period_ms))
    {
        return 0u;
    }
    x = (elapsed_ms < half) ? ((elapsed_ms * PM) / half)
                            : (((period_ms - elapsed_ms) * PM) / half);
    x = (x > PM) ? PM : x;
    return (x * x * ((3u * PM) - (2u * x))) / (PM * PM);
}

void lamp_anim_init(lamp_anim_t *p_a)
{
    *p_a = (lamp_anim_t){ 0 };
}

uint16_t lamp_anim_base(const lamp_anim_t *p_a, uint32_t now_ms)
{
    const uint32_t elapsed = now_ms - p_a->fade_t0_ms;
    int32_t        delta;

    if (elapsed >= p_a->fade_ms)
    {
        return p_a->to_pm;
    }
    delta = (int32_t)p_a->to_pm - (int32_t)p_a->from_pm;
    return (uint16_t)((int32_t)p_a->from_pm +
                      (int32_t)(((int64_t)delta * (int64_t)elapsed) /
                                (int64_t)p_a->fade_ms));
}

void lamp_anim_fade_to(lamp_anim_t *p_a, uint16_t target_pm, uint32_t dur_ms,
                       uint32_t now_ms)
{
    p_a->from_pm    = lamp_anim_base(p_a, now_ms);   /* start anew from here */
    p_a->to_pm      = clamp_pm(target_pm);
    p_a->fade_t0_ms = now_ms;
    p_a->fade_ms    = dur_ms;
}

void lamp_anim_pulse(lamp_anim_t *p_a, uint32_t now_ms)
{
    p_a->b_pulse     = true;
    p_a->pulse_t0_ms = now_ms;
}

void lamp_anim_breathe(lamp_anim_t *p_a, bool b_on, uint32_t now_ms)
{
    if (b_on)
    {
        if (!p_a->b_breathe)
        {
            p_a->breathe_t0_ms = now_ms;
        }
        p_a->b_breathe      = true;
        p_a->b_breathe_stop = false;
    }
    else if (p_a->b_breathe && !p_a->b_breathe_stop)
    {
        /* End at the next period boundary: the current dip completes */
        const uint32_t elapsed = now_ms - p_a->breathe_t0_ms;
        const uint32_t periods = (elapsed / LAMP_ANIM_BREATHE_MS) + 1u;

        p_a->b_breathe_stop = true;
        p_a->breathe_end_ms = p_a->breathe_t0_ms + (periods * LAMP_ANIM_BREATHE_MS);
    }
    else
    {
        /* already off or stopping */
    }
}

uint16_t lamp_anim_level(lamp_anim_t *p_a, uint32_t now_ms)
{
    const uint32_t base  = lamp_anim_base(p_a, now_ms);
    uint32_t       level = base;

    if (p_a->b_breathe)
    {
        if (p_a->b_breathe_stop &&
            ((int32_t)(now_ms - p_a->breathe_end_ms) >= 0))
        {
            p_a->b_breathe      = false;
            p_a->b_breathe_stop = false;
        }
        else
        {
            const uint32_t phase = (now_ms - p_a->breathe_t0_ms) % LAMP_ANIM_BREATHE_MS;
            const uint32_t env   = bump_pm(phase, LAMP_ANIM_BREATHE_MS);

            level -= (level * LAMP_ANIM_BREATHE_DEPTH_PM * env) / (PM * PM);
        }
    }

    if (p_a->b_pulse)
    {
        const uint32_t elapsed = now_ms - p_a->pulse_t0_ms;

        if (elapsed >= LAMP_ANIM_PULSE_MS)
        {
            p_a->b_pulse = false;
        }
        else
        {
            const uint32_t swing =
                (LAMP_ANIM_PULSE_PM * bump_pm(elapsed, LAMP_ANIM_PULSE_MS)) / PM;

            if (base < (LAMP_ANIM_MAX_PM / 2u))
            {
                level += swing;
            }
            else
            {
                level = (level > swing) ? (level - swing) : 0u;
            }
        }
    }
    return clamp_pm(level);
}
