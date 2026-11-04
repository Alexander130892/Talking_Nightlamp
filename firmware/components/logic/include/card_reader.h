/**
 * @file  card_reader.h
 * @brief Turns raw RFID polls into card_fsm input: record cache, retries.
 *
 * Pure logic, host-tested (test/host/test_card_reader.c). The RFID task
 * calls card_reader_step() once per poll; reading the record is done via a
 * callback so the hardware stays out of this module.
 *
 * Contract
 *  - NONE (no tag): output absent; the cache is cleared, so a card that is
 *    put back is read again (it may have been rewritten).
 *  - COLLISION (two cards): output present + invalid, uid_len 0 (F13).
 *  - GARBLED (a tag answered but could not be selected, e.g. half lifted):
 *    no information. The previous output is repeated: nothing changed.
 *  - TAG with the cached UID: output from the cache, no read.
 *  - TAG with a new UID: read once.
 *      read OK, record valid   -> cache + output valid (story, hue)
 *      read OK, record invalid -> cache + output invalid (foreign card, F9)
 *      read failed             -> previous output repeated, retried on the
 *                                 next poll; after CARD_READER_RETRIES
 *                                 failures in a row the UID is cached as
 *                                 invalid (a card with another key, F9).
 *  - A failed read therefore never sounds the unknown chime by itself.
 */

#ifndef CARD_READER_H
#define CARD_READER_H

#include <stdbool.h>
#include <stdint.h>
#include "card_fsm.h"

#define CARD_READER_RETRIES   (3u)
#define CARD_READER_BLOCK_LEN (16u)

typedef enum
{
    CARD_RD_NONE = 0,
    CARD_RD_COLLISION,
    CARD_RD_GARBLED,
    CARD_RD_TAG,
} card_rd_t;

/** Read the record of the tag that was just polled. true = 16 bytes read. */
typedef bool (*card_read_fn_t)(void *p_ctx, uint8_t p_buf[CARD_READER_BLOCK_LEN]);

typedef struct
{
    bool        b_cached;
    card_poll_t cached;      /* UID + parsed result of the cached card */
    uint8_t     fail_uid[CARD_UID_MAX_LEN];
    uint8_t     fail_uid_len;
    uint8_t     fails;
    card_poll_t last;        /* last output, repeated when there is no info */
} card_reader_t;

void card_reader_init(card_reader_t *p_rd);

/**
 * One poll.
 * @param status   what the poll saw
 * @param p_uid    UID for CARD_RD_TAG (ignored otherwise)
 * @param read_fn  called only for a new UID
 * @return the card_fsm input for this poll
 */
card_poll_t card_reader_step(card_reader_t *p_rd, card_rd_t status,
                             const uint8_t *p_uid, uint8_t uid_len,
                             card_read_fn_t read_fn, void *p_ctx);

#endif /* CARD_READER_H */
