/**
 * @file  audio.h
 * @brief Stories and chime: SD -> minimp3 -> I2S0 -> MAX98357A.
 *
 * Rationale: ADR 0002 (pipeline), 0009 (volume layers), 0013 (minimp3).
 * Proven in bring-up 08 (pop-free start), 08b (decode, 10 % CPU), 11d.
 *
 * All control functions only post a command to the audio task and return at
 * once; results come back as events (audio_get_event). The driver has no
 * policy: what to play when is decided by the app (card_fsm).
 *
 * Behaviour
 *  - A story is all .mp3 parts of /stories/NNN in name order (storage.h).
 *  - Every stop of sound (pause, stop, story switch, chime interruption)
 *    fades out over AUDIO_FADE_MS; every (re)start fades in. No clicks.
 *  - Pause keeps story, part and byte offset. Resume starts AUDIO_REWIND_MS
 *    before that point (estimated from the bitrate), so the listener hears
 *    the faded words again; the decoder resyncs on the next frame header.
 *  - Chime during a story: story fades out, chime plays, story resumes
 *    (same rewind). Chime while paused or idle: plays, state unchanged.
 *  - Mute: the story keeps running silently (F14). Mute and volume ramp.
 *  - Story end: the DMA buffer is drained, then AUDIO_EVT_STORY_DONE.
 *  - Volume steps 0..AUDIO_VOLUME_STEPS-1: 0 = silent, then
 *    AUDIO_STEP_DB per step up to max_gain_db at the top step (ADR 0009).
 *  - DMA holds AUDIO_DMA_MS of audio: enough to ride out a slow SD read.
 *    Every closed file logs its DMA underruns and slowest SD read.
 */

#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"

#define AUDIO_VOLUME_STEPS (16u)   /* 0..15 */
#define AUDIO_STEP_DB      (2)   /* 30 dB range: the bottom of 45 dB was unusable */
#define AUDIO_FADE_MS      (80u)
#define AUDIO_REWIND_MS    (1500u)
#define AUDIO_DMA_MS       (93u)  /* 8 x 512 frames at 44.1 kHz */
#define AUDIO_CHIME_PATH   "/sd/system/unknown.mp3"

typedef enum
{
    AUDIO_EVT_STORY_DONE,   /* last part played out */
    AUDIO_EVT_STORY_ERROR,  /* story missing or unreadable, now idle */
} audio_evt_type_t;

typedef struct
{
    audio_evt_type_t type;
    uint8_t          story_id;
} audio_event_t;

typedef struct
{
    gpio_num_t bclk;
    gpio_num_t ws;
    gpio_num_t dout;
    gpio_num_t amp_sd;        /* low = shutdown */
    int8_t     max_gain_db;   /* gain at the top volume step */
    uint8_t    start_volume;  /* 0..AUDIO_VOLUME_STEPS-1 */
    uint32_t   task_prio;
    int        core;          /* 1: away from the system tasks on core 0 */
} audio_config_t;

/**
 * Pop-free start: amp shut down, I2S running with silence, amp enabled.
 * Then starts the audio task. Call before storage_init(): the amp must be
 * silent before anything else powers up.
 */
esp_err_t audio_init(const audio_config_t *p_cfg);

void audio_play_story(uint8_t story_id);   /* from the beginning */
void audio_pause(void);
void audio_resume(void);
void audio_stop(void);                      /* forget the position */
void audio_play_chime(void);
void audio_set_volume(uint8_t step);        /* clamped */
void audio_set_mute(bool b_mute);

/** Next event, waiting up to timeout_ms. @return true if *p_evt filled. */
bool audio_get_event(audio_event_t *p_evt, uint32_t timeout_ms);

#endif /* AUDIO_H */
