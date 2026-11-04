/**
 * @file  storage.h
 * @brief microSD on SPI (SDSPI + FATFS) and story folder lookup.
 *
 * Layout: docs/content/audio-content.md (MP3 files in /stories/NNN/).
 * Proven in bring-up 07 (mount, speed) and 08b (listing, dot files).
 *
 * Contract
 *  - This firmware NEVER writes to the card: nothing here opens a file for
 *    writing, and the mount never formats (format_if_mount_failed = false).
 *    Pulling the plug mid-story can therefore not corrupt the card (N4, E12).
 *  - A story's parts are its .mp3 files (case-insensitive extension) in
 *    name order. Names starting with '.' (macOS ._ files) are skipped.
 *  - The part list of one story is cached: it is read and sorted when a
 *    different story_id is asked for. Valid because the card cannot change
 *    while the lamp is powered (it sits inside the enclosure).
 *  - At most STORAGE_MAX_PARTS parts per story; longer names than
 *    STORAGE_MAX_NAME - 1 characters are skipped (and logged).
 *  - Errors are only reported; what the child sees is the app's decision.
 */

#ifndef STORAGE_H
#define STORAGE_H

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/spi_common.h"

#define STORAGE_MOUNT_POINT "/sd"
#define STORAGE_MAX_PARTS   (16u)
#define STORAGE_MAX_NAME    (64u)
#define STORAGE_PATH_LEN    (96u)   /* enough for any part path */

typedef struct
{
    spi_host_device_t host;
    gpio_num_t        sck;
    gpio_num_t        miso;
    gpio_num_t        mosi;
    gpio_num_t        cs;
    int               freq_khz;     /* 10000 proven in bring-up 07 */
} storage_config_t;

/**
 * Mount the card at STORAGE_MOUNT_POINT.
 * @return ESP_OK, or the mount error (no card, not FAT32, ...).
 */
esp_err_t storage_init(const storage_config_t *p_cfg);

/** ESP_OK if /stories/NNN exists and holds at least one .mp3. */
esp_err_t storage_story_exists(uint8_t story_id);

/**
 * Path of part `part` (0-based, name order) of a story.
 * @return ESP_ERR_NOT_FOUND after the last part (or no such story),
 *         ESP_ERR_INVALID_SIZE if len is too small.
 */
esp_err_t storage_story_part_path(uint8_t story_id, uint8_t part,
                                  char *p_path, size_t len);

#endif /* STORAGE_H */
