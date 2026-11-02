/**
 * @file  input.h
 * @brief Lamp button, rotary encoder and encoder push switch.
 *
 * The driver runs its own task: buttons are polled every INPUT_POLL_MS and
 * accepted after INPUT_DEBOUNCE_SAMPLES equal samples; encoder A/B are
 * decoded in hardware by the PCNT peripheral (quadrature x4, glitch filter).
 * Events go into a FIFO queue that the app reads with input_get_event().
 *
 * Encoder A/B are on input-only GPIO 34/35 with external 10 kOhm pull-ups
 * and 10 nF to GND (hardware RC filter).
 *
 * Queue-full policy:
 *  - Encoder steps are never lost: they accumulate in the driver and are
 *    posted as one INPUT_EVT_ENC_STEP (steps summed) once there is room.
 *  - A press that finds the queue full is dropped and logged. With the
 *    queue depth and human press rates this means the app is stuck.
 */

#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"

#define INPUT_POLL_MS          (10u)
#define INPUT_DEBOUNCE_SAMPLES (3u)    /* 30 ms */
#define INPUT_QUEUE_DEPTH      (8u)

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

typedef struct
{
    gpio_num_t lamp_btn;        /* active low, internal pull-up */
    gpio_num_t enc_sw;          /* active low, internal pull-up */
    gpio_num_t enc_a;           /* external pull-up */
    gpio_num_t enc_b;           /* external pull-up */
    int        counts_per_step; /* PCNT counts per detent (EC11: 4) */
    uint32_t   task_prio;
} input_config_t;

/**
 * Configure pins and PCNT, start the input task.
 * @return ESP_ERR_INVALID_ARG on a bad config, ESP_ERR_INVALID_STATE if
 *         already initialised, or the driver error.
 */
esp_err_t input_init(const input_config_t *p_cfg);

/**
 * Get the next event, waiting up to timeout_ms.
 * @return true if *p_evt was filled.
 */
bool input_get_event(input_event_t *p_evt, uint32_t timeout_ms);

#endif /* INPUT_H */
