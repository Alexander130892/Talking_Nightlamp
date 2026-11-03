/**
 * @file  lamp_anim.h
 * @brief Lamp brightness over time: fades, soft pulse, breathing.
 *
 * Pure logic, host-tested (test/host/test_lamp_anim.c). The lamp task calls
 * lamp_anim_level() once per frame and renders the result with lamp_model.
 * All times are a free-running millisecond counter; differences are taken
 * unsigned, so counter wrap-around (49 days) is harmless.
 *
 * Contract
 *  - Base level: lamp_anim_fade_to() starts a linear fade FROM the base level
 *    shown at that moment (an interrupted fade never jumps) TO target_pm in
 *    dur_ms. dur_ms = 0 sets the level at once.
 *  - Pulse (F9): one soft swing of LAMP_ANIM_PULSE_PM away from the base
 *    level and back, LAMP_ANIM_PULSE_MS long: up when the base is below 50 %,
 *    down otherwise, so it is visible at any level, including off.
 *  - Breathing (F2): repeated dips of LAMP_ANIM_BREATHE_DEPTH_PM of the base
 *    level, period LAMP_ANIM_BREATHE_MS. Stopping lets the current dip finish.
 *  - Effects modulate the output only; the base level and fades continue
 *    underneath. Output is always 0..LAMP_ANIM_MAX_PM.
 *  - Envelopes are smooth (smoothstep): no visible corners at start or end.
 */

#ifndef LAMP_ANIM_H
#define LAMP_ANIM_H

#include <stdbool.h>
#include <stdint.h>

#define LAMP_ANIM_MAX_PM           (1000u)
#define LAMP_ANIM_PULSE_PM         (250u)
#define LAMP_ANIM_PULSE_MS         (800u)
#define LAMP_ANIM_BREATHE_DEPTH_PM (400u)   /* dip to 60 % of the base */
#define LAMP_ANIM_BREATHE_MS       (2000u)

typedef struct
{
    uint16_t from_pm;
    uint16_t to_pm;
    uint32_t fade_t0_ms;
    uint32_t fade_ms;
    bool     b_pulse;
    uint32_t pulse_t0_ms;
    bool     b_breathe;
    bool     b_breathe_stop;
    uint32_t breathe_t0_ms;
    uint32_t breathe_end_ms;   /* valid when b_breathe_stop */
} lamp_anim_t;

/** Lamp off, no effects. */
void lamp_anim_init(lamp_anim_t *p_a);

void lamp_anim_fade_to(lamp_anim_t *p_a, uint16_t target_pm, uint32_t dur_ms,
                       uint32_t now_ms);

void lamp_anim_pulse(lamp_anim_t *p_a, uint32_t now_ms);

void lamp_anim_breathe(lamp_anim_t *p_a, bool b_on, uint32_t now_ms);

/** Base level (fade only, no effects) at now_ms. */
uint16_t lamp_anim_base(const lamp_anim_t *p_a, uint32_t now_ms);

/** Output level at now_ms. Ends finished effects, hence non-const. */
uint16_t lamp_anim_level(lamp_anim_t *p_a, uint32_t now_ms);

#endif /* LAMP_ANIM_H */
