/**
 * @file  rc522.h
 * @brief MFRC522 driver on SPI3: presence polling and record read/write.
 *
 * Polling scheme: ADR 0004 (WUPA -> anticollision/SELECT -> HLTA).
 * Tag types: ADR 0005 (MIFARE Classic 1K block 4, NTAG21x page 4).
 */

#ifndef RC522_H
#define RC522_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define RC522_UID_MAX_LEN (10u)
#define RC522_SAK_ULTRALIGHT (0x00u) /* NTAG21x */
#define RC522_SAK_CLASSIC_1K (0x08u)

typedef struct
{
    uint8_t uid[RC522_UID_MAX_LEN];
    uint8_t uid_len;
    uint8_t sak;
} rc522_tag_t;

/** Reset the chip, check VersionReg, enable the antenna. */
esp_err_t rc522_init(void);

/**
 * One presence poll: WUPA, select, HLTA. HLTA is sent whenever WUPA was
 * answered, also if select failed, so no card is left in READY (ADR 0004).
 * @return ESP_OK                    *p_tag filled
 *         ESP_ERR_NOT_FOUND         no tag answered
 *         ESP_ERR_INVALID_RESPONSE  a tag answered but none could be selected
 *                                   (collision: two cards, BCC/CRC error)
 */
esp_err_t rc522_poll(rc522_tag_t *p_tag);

/** Read the 16-byte record area of the selected tag (block 4 / pages 4-7). */
esp_err_t rc522_read_record(const rc522_tag_t *p_tag, uint8_t p_buf[16]);

/** Write the record area. Used by the `write` console command. */
esp_err_t rc522_write_record(const rc522_tag_t *p_tag, const uint8_t p_buf[16]);

#endif /* RC522_H */
