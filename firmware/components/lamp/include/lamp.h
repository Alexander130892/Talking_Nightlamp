/**
 * @file  lamp.h
 * @brief Lamp task: commands in, 50 fps frames out.
 *
 * Per frame (LAMP_FRAME_MS):
 *   lamp_anim (fade + effects) -> lamp_model (gamma, 12->6->3->1)
 *   -> lamp_tint (F15) -> power_limit (150 mA) -> lamp_strip_show
 *
 * The functions below only post a command to the lamp task's queue and
 * return at once, so any task may call them. If the queue is full the
 * command is dropped and logged (depth LAMP_CMD_QUEUE_DEPTH; at human
 * event rates this means the lamp task is stuck).
 */

#ifndef LAMP_H
#define LAMP_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "lamp_strip.h"

#define LAMP_FRAME_MS        (20u)
#define LAMP_CMD_QUEUE_DEPTH (8u)

typedef struct
{
    lamp_strip_config_t strip;
    uint32_t            task_prio;
} lamp_config_t;

/** Init the strip (all off) and start the lamp task. */
esp_err_t lamp_start(const lamp_config_t *p_cfg);

/** Fade from the level shown now to level_pm (0..1000) in dur_ms. */
void lamp_fade_to(uint16_t level_pm, uint32_t dur_ms);

/** One soft pulse (F9). */
void lamp_pulse(void);

/** Breathing on/off (F2 grace signal). Off lets the current dip finish. */
void lamp_breathe(bool b_on);

/** Card colour tint (F15). strength_pm 0 = plain warm white. */
void lamp_set_tint(uint8_t hue, uint16_t strength_pm);

/** Level the base fade is at right now (for the app, e.g. lamp toggle). */
uint16_t lamp_get_level(void);

#endif /* LAMP_H */
