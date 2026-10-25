/**
 * @file  storage.h
 * @brief microSD on SPI2 (SDSPI + FATFS) and story folder lookup.
 *
 * Layout: docs/content/audio-content.md (MP3 files in /stories/NNN/).
 */

#ifndef STORAGE_H
#define STORAGE_H

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define STORAGE_MOUNT_POINT "/sd"

/** Mount the card read-only at STORAGE_MOUNT_POINT. */
esp_err_t storage_init(void);

/** ESP_OK if /stories/NNN exists and holds at least one .mp3. */
esp_err_t storage_story_exists(uint8_t story_id);

/**
 * Path of part `part` (0-based, name order) of a story.
 * @return ESP_ERR_NOT_FOUND after the last part.
 */
esp_err_t storage_story_part_path(uint8_t story_id, uint8_t part,
                                  char *p_path, size_t len);

#endif /* STORAGE_H */
