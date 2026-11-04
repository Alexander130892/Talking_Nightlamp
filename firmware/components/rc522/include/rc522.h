/**
 * @file  rc522.h
 * @brief MFRC522 driver on SPI: presence polling and record read/write.
 *
 * Polling scheme: ADR 0004 (WUPA -> anticollision/SELECT -> HLTA).
 * Tag types: ADR 0005 (MIFARE Classic 1K block 4, NTAG21x pages 4-7).
 * Proven in bring-up 02-06 (Classic 1K). NTAG paths are untested on
 * hardware until an NTAG card is tried.
 *
 * Every session ends with HLTA and Crypto1 cleared, on every path, so no
 * card is left READY or authenticated (bring-up 05/06 lessons).
 * Not thread-safe: call from one task only (the RFID task).
 */

#ifndef RC522_H
#define RC522_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/spi_common.h"

#define RC522_UID_MAX_LEN    (10u)
#define RC522_RECORD_LEN     (16u)
#define RC522_SAK_ULTRALIGHT (0x00u) /* NTAG21x */
#define RC522_SAK_CLASSIC_1K (0x08u)

typedef struct
{
    uint8_t uid[RC522_UID_MAX_LEN];
    uint8_t uid_len;
    uint8_t sak;
} rc522_tag_t;

typedef struct
{
    spi_host_device_t host;
    gpio_num_t        sck;
    gpio_num_t        miso;
    gpio_num_t        mosi;
    gpio_num_t        cs;
    gpio_num_t        rst;
    int               clock_hz;   /* 1 MHz proven in bring-up */
} rc522_config_t;

/**
 * Reset the chip, check VersionReg, enable the antenna.
 * @return ESP_ERR_NOT_FOUND if no chip answers (VersionReg 0x00 / 0xFF).
 */
esp_err_t rc522_init(const rc522_config_t *p_cfg);

/**
 * One presence poll: WUPA, select, HLTA.
 * @return ESP_OK                    *p_tag filled
 *         ESP_ERR_NOT_FOUND         no tag answered
 *         ESP_ERR_INVALID_RESPONSE  collision: two or more tags (F13)
 *         ESP_ERR_INVALID_CRC       a tag answered but the exchange was
 *                                   garbled (BCC/CRC/parity, timeout): retry
 */
esp_err_t rc522_poll(rc522_tag_t *p_tag);

/**
 * Wake and select p_tag again, read the 16-byte record area
 * (Classic: block 4 with key A FF..FF; NTAG: pages 4-7).
 * @return ESP_OK, ESP_ERR_NOT_FOUND (tag gone or another tag answered),
 *         ESP_ERR_INVALID_STATE (authentication failed: other key),
 *         ESP_ERR_INVALID_CRC (garbled), ESP_ERR_NOT_SUPPORTED (tag type).
 */
esp_err_t rc522_read_record(const rc522_tag_t *p_tag, uint8_t p_buf[RC522_RECORD_LEN]);

/**
 * Write the record area and verify by reading it back. Only block 4 /
 * pages 4-7 can ever be written (whitelist). Used by the console command.
 */
esp_err_t rc522_write_record(const rc522_tag_t *p_tag,
                             const uint8_t p_buf[RC522_RECORD_LEN]);

#endif /* RC522_H */
