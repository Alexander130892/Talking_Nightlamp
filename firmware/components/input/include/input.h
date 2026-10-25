/**
 * @file  input.h
 * @brief Lamp button and rotary encoder (with push switch).
 *
 * Encoder A/B are on input-only GPIO 34/35 with external 10 kOhm pull-ups
 * and 10 nF to GND (hardware RC debounce).
 */

#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum
{
    INPUT_EVT_NONE = 0,
    INPUT_EVT_LAMP_PRESS,
    INPUT_EVT_ENC_PRESS,
    INPUT_EVT_ENC_STEP /* see input_event_t.steps */
} input_evt_type_t;

typedef struct
{
    input_evt_type_t type;
    int8_t           steps; /* +cw / -ccw, for INPUT_EVT_ENC_STEP */
} input_event_t;

esp_err_t input_init(void);

/**
 * Get the next debounced event.
 * @return true if *p_evt was filled.
 */
bool input_get_event(input_event_t *p_evt, uint32_t timeout_ms);

#endif /* INPUT_H */
