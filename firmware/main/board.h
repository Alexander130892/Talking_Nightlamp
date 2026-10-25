/**
 * @file  board.h
 * @brief Pin assignments for the ESP32-WROOM-32 30-pin devkit.
 *
 * Single source of truth for GPIO numbers. Keep in sync with
 * docs/hardware/pin-mapping.md.
 */

#ifndef BOARD_H
#define BOARD_H

/* RC522 on SPI3 (VSPI) */
#define BOARD_RC522_SCK      (18)
#define BOARD_RC522_MISO     (19)
#define BOARD_RC522_MOSI     (23)
#define BOARD_RC522_CS       (5)  /* strapping pin, idles high */
#define BOARD_RC522_RST      (22)

/* microSD on SPI2 (HSPI) through the GPIO matrix.
 * MISO is NOT on GPIO 12: a card pulls MISO high, and GPIO 12 high at
 * boot selects 1.8 V flash. */
#define BOARD_SD_SCK         (14)
#define BOARD_SD_MISO        (27)
#define BOARD_SD_MOSI        (13)
#define BOARD_SD_CS          (4)

/* MAX98357A on I2S0 */
#define BOARD_I2S_BCLK       (26)
#define BOARD_I2S_LRC        (25)
#define BOARD_I2S_DIN        (33)
#define BOARD_AMP_SD         (21) /* low = shutdown, high = on (left ch) */

/* SK6812 RGBW strip via 74AHCT125, driven by RMT */
#define BOARD_LED_DATA       (32)
#define BOARD_LED_COUNT      (12)

/* Controls */
#define BOARD_BTN_LAMP       (16) /* internal pull-up, active low */
#define BOARD_ENC_SW         (17) /* internal pull-up, active low */
#define BOARD_ENC_A          (34) /* input-only: external 10k pull-up */
#define BOARD_ENC_B          (35) /* input-only: external 10k pull-up */

#endif /* BOARD_H */
