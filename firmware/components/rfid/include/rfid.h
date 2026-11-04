/**
 * @file  rfid.h
 * @brief RFID task: polls the RC522 every RFID_PERIOD_MS and turns each poll
 *        into a card_fsm input (card_reader: record cache, read retries).
 *
 * Only the LATEST poll result is kept (queue of one, overwritten): if the
 * app is late, it sees the newest state, never a backlog. A skipped result
 * is the same as a skipped poll, which card_fsm tolerates by design.
 */

#ifndef RFID_H
#define RFID_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "card_fsm.h"
#include "rc522.h"

#define RFID_PERIOD_MS (150u)

typedef struct
{
    rc522_config_t rc522;
    uint32_t       task_prio;
} rfid_config_t;

/** Init the RC522 and start polling. */
esp_err_t rfid_start(const rfid_config_t *p_cfg);

/** Latest poll result, waiting up to timeout_ms. @return true if filled. */
bool rfid_get_poll(card_poll_t *p_poll, uint32_t timeout_ms);

#endif /* RFID_H */
