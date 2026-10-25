/**
 * @file  audio.h
 * @brief MP3 decode -> I2S0 -> MAX98357A. Owns the amplifier shutdown pin.
 *
 * Rationale: ADR 0002 (pipeline), ADR 0009 (volume layers).
 */

#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define AUDIO_VOLUME_STEPS (16u) /* encoder steps 0..15 */

/** Configure I2S and drive the amp shutdown pin low (muted, no pop). */
esp_err_t audio_init(void);

/** Start a story from the beginning (all parts in name order). */
esp_err_t audio_play_story(uint8_t story_id);

esp_err_t audio_pause(void);
esp_err_t audio_resume(void);
esp_err_t audio_stop(void);

/** Play a short system sound (e.g. /system/unknown.mp3) over the story. */
esp_err_t audio_play_chime(void);

/** 0..AUDIO_VOLUME_STEPS-1, clamped to the firmware maximum. */
esp_err_t audio_set_volume(uint8_t step);

/** True once the last part of the current story has played out. */
bool audio_story_finished(void);

#endif /* AUDIO_H */
