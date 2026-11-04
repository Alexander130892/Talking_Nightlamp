/**
 * @file  audio.c
 * @brief Audio task (see audio.h for the behaviour contract).
 *
 * Design: commands only change the WANTED state (story state, chime
 * request, volume). Once per MP3 frame the task reconciles what is wanted
 * with what is open:
 *
 *   wanted source = CHIME   if a chime is requested or playing
 *                 = STORY   if the story state is PLAYING
 *                 = NONE    otherwise
 *
 *   open source differs from wanted -> fade the gain to 0 (if needed),
 *   close it (saving the story position), open the wanted one.
 *
 * One gain ramp handles fades, volume changes and mute: the gain moves
 * towards its target by a fixed step per sample, AUDIO_FADE_MS for 0 dB.
 */

#include "audio.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "minimp3.h"
#include "storage.h"

#define DEFAULT_RATE   (44100u)
#define IN_BUF_BYTES   (8192u)
#define REFILL_BELOW   (2048u)          /* > largest MP3 frame (1441 B) */
#define TASK_STACK     (32u * 1024u)    /* minimp3 needs ~16 KB of stack */
#define CMD_DEPTH      (8u)
#define EVT_DEPTH      (4u)
#define UNITY_Q16      (65536)
#define DMA_DESC_NUM   (8u)
#define DMA_FRAME_NUM  (512u)           /* 8 x 512 frames = 93 ms at 44.1 kHz */
#define DRAIN_MS       (AUDIO_DMA_MS + 20u)

static const char *TAG = "audio";

/* ---- Commands and state --------------------------------------------- */

typedef enum
{
    CMD_PLAY, CMD_PAUSE, CMD_RESUME, CMD_STOP, CMD_CHIME, CMD_VOLUME, CMD_MUTE
} cmd_type_t;

typedef struct
{
    cmd_type_t type;
    uint8_t    arg;    /* story id, volume step or mute flag */
} cmd_t;

typedef enum { STORY_IDLE, STORY_PLAYING, STORY_PAUSED } story_state_t;
typedef enum { SRC_NONE, SRC_STORY, SRC_CHIME } src_t;

typedef struct
{
    story_state_t state;
    uint8_t       id;
    uint8_t       part;
    long          offset;   /* byte offset of the next frame in the part */
    uint32_t      gen;      /* bumped on every new play / resume */
} story_t;

static audio_config_t    s_cfg;
static QueueHandle_t     s_cmds;
static QueueHandle_t     s_evts;
static i2s_chan_handle_t s_tx;
static uint32_t          s_rate_hz;

/* Wanted */
static story_t s_story;
static bool    s_b_chime_req;
static uint8_t s_volume;
static bool    s_b_mute;

/* Open source */
static src_t    s_open      = SRC_NONE;
static uint32_t s_open_gen;
static FILE    *s_f;
static long     s_buf_off;      /* file offset of s_in[0] */
static size_t   s_pos;          /* next frame inside s_in */
static size_t   s_avail;        /* bytes from s_pos on */
static bool     s_b_eof;
static bool     s_b_first;      /* no PCM frame decoded yet */
static int      s_kbps;         /* last bitrate seen, for the rewind */

/* Gain ramp, Q16 (65536 = 0 dB) */
static int32_t s_gain;
static int32_t s_gain_target;
static int32_t s_ramp_step;

/* Diagnostics, per open file */
static volatile uint32_t s_underruns;    /* counted in the I2S ISR */
static uint32_t          s_underruns_at_open;
static bool              s_b_snap;       /* take the snapshot after 1st write */
static int64_t           s_max_read_us;

/* Buffers */
static mp3dec_t s_dec;
static uint8_t  s_in[IN_BUF_BYTES];
static int16_t  s_pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
static int16_t  s_out[MINIMP3_MAX_SAMPLES_PER_FRAME];

/* ---- Amp and I2S ----------------------------------------------------- */

static void amp_enable(bool b_on)
{
    (void)gpio_set_level(s_cfg.amp_sd, b_on ? 1 : 0);
}

static void write_silence(uint32_t ms)
{
    const uint32_t frames = (s_rate_hz * ms) / 1000u;
    size_t         written;

    (void)memset(s_out, 0, sizeof(s_out));
    for (uint32_t done = 0u; done < frames; done += (sizeof(s_out) / 4u))
    {
        (void)i2s_channel_write(s_tx, s_out, sizeof(s_out), &written, portMAX_DELAY);
    }
}

/** I2S ISR: the DMA ran out of new data and sent zeros (auto_clear). */
static bool IRAM_ATTR on_underrun(i2s_chan_handle_t h, i2s_event_data_t *p_e, void *p_ctx)
{
    (void)h;
    (void)p_e;
    (void)p_ctx;
    s_underruns++;
    return false;
}

static void update_ramp_step(void)
{
    /* full scale (0 dB) in AUDIO_FADE_MS */
    s_ramp_step = (int32_t)(((uint32_t)UNITY_Q16 * 1000u) / (s_rate_hz * AUDIO_FADE_MS));
    s_ramp_step = (s_ramp_step < 1) ? 1 : s_ramp_step;
}

static esp_err_t i2s_start(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    const i2s_std_config_t std_cfg =
    {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(DEFAULT_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                        I2S_SLOT_MODE_STEREO),
        .gpio_cfg =
        {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = s_cfg.bclk,
            .ws   = s_cfg.ws,
            .dout = s_cfg.dout,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = { false, false, false },
        },
    };

    const i2s_event_callbacks_t cbs = { .on_send_q_ovf = on_underrun };

    chan_cfg.auto_clear    = true;   /* underrun -> zeros, not a repeated buffer */
    chan_cfg.dma_desc_num  = DMA_DESC_NUM;
    chan_cfg.dma_frame_num = DMA_FRAME_NUM;
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &s_tx, NULL), TAG, "channel");
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_tx, &std_cfg), TAG, "std mode");
    ESP_RETURN_ON_ERROR(i2s_channel_register_event_callback(s_tx, &cbs, NULL), TAG, "cb");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(s_tx), TAG, "enable");
    s_rate_hz = DEFAULT_RATE;
    update_ramp_step();
    return ESP_OK;
}

/** Change sample rate with the amp muted, so the switch can't click. */
static void i2s_set_rate(uint32_t rate_hz)
{
    const i2s_std_clk_config_t clk = I2S_STD_CLK_DEFAULT_CONFIG(rate_hz);

    if (rate_hz == s_rate_hz)
    {
        return;
    }
    amp_enable(false);
    (void)i2s_channel_disable(s_tx);
    (void)i2s_channel_reconfig_std_clock(s_tx, &clk);
    (void)i2s_channel_enable(s_tx);
    s_rate_hz = rate_hz;
    update_ramp_step();
    write_silence(30u);
    amp_enable(true);
}

/* ---- Gain ------------------------------------------------------------ */

static int32_t volume_gain(void)
{
    float db;

    if (s_b_mute || (0u == s_volume))
    {
        return 0;
    }
    db = (float)s_cfg.max_gain_db -
         (float)(AUDIO_STEP_DB * (int)((AUDIO_VOLUME_STEPS - 1u) - s_volume));
    return (int32_t)((float)UNITY_Q16 * powf(10.0f, db / 20.0f));
}

/* ---- Sources --------------------------------------------------------- */

static void post_event(audio_evt_type_t type, uint8_t id)
{
    const audio_event_t evt = { .type = type, .story_id = id };

    if (pdTRUE != xQueueSend(s_evts, &evt, 0))
    {
        ESP_LOGW(TAG, "event %d dropped", (int)type);
    }
}

static void log_file_stats(void)
{
    ESP_LOGI(TAG, "  closed: %lu underruns, slowest SD read %lld ms",
             (unsigned long)(s_underruns - s_underruns_at_open), s_max_read_us / 1000);
}

static void close_source(void)
{
    log_file_stats();
    if ((SRC_STORY == s_open) && (s_open_gen == s_story.gen))
    {
        /* Same story still wanted (paused or interrupted by the chime):
         * remember where we are */
        s_story.offset = s_buf_off + (long)s_pos;
    }
    if (NULL != s_f)
    {
        (void)fclose(s_f);
        s_f = NULL;
    }
    s_open = SRC_NONE;
}

static bool open_file(const char *p_path, long offset)
{
    s_f = fopen(p_path, "rb");
    if (NULL == s_f)
    {
        ESP_LOGE(TAG, "cannot open %s", p_path);
        return false;
    }
    if ((offset > 0) && (0 != fseek(s_f, offset, SEEK_SET)))
    {
        offset = 0;
        (void)fseek(s_f, 0, SEEK_SET);
    }
    mp3dec_init(&s_dec);
    s_buf_off = offset;
    s_pos     = 0u;
    s_avail   = 0u;
    s_b_eof   = false;
    s_b_first = true;
    s_b_snap      = true;    /* DMA may be running empty until the first write */
    s_max_read_us = 0;
    ESP_LOGI(TAG, "open %s @ %ld", p_path, offset);
    return true;
}

/** Open part s_story.part at s_story.offset. False: story is over/broken. */
static bool open_story_part(void)
{
    char path[STORAGE_PATH_LEN];

    if (ESP_OK != storage_story_part_path(s_story.id, s_story.part, path, sizeof(path)))
    {
        return false;
    }
    return open_file(path, s_story.offset);
}

static void open_wanted(src_t want)
{
    if (SRC_CHIME == want)
    {
        s_b_chime_req = false;
        if (open_file(AUDIO_CHIME_PATH, 0))
        {
            s_open = SRC_CHIME;
            s_gain = volume_gain();          /* keep the bell's attack */
        }
    }
    else if (SRC_STORY == want)
    {
        if (open_story_part())
        {
            s_open     = SRC_STORY;
            s_open_gen = s_story.gen;
            s_gain     = 0;                  /* fade in */
        }
        else
        {
            ESP_LOGE(TAG, "story %03u: no playable part", (unsigned)s_story.id);
            s_story.state = STORY_IDLE;
            post_event(AUDIO_EVT_STORY_ERROR, s_story.id);
        }
    }
    else
    {
        /* nothing wanted */
    }
}

/** End of the open file reached. */
static void source_finished(void)
{
    const src_t was     = s_open;
    const bool  b_stale = (SRC_STORY == was) && (s_open_gen != s_story.gen);

    log_file_stats();
    (void)fclose(s_f);
    s_f    = NULL;
    s_open = SRC_NONE;

    if (b_stale)
    {
        return;   /* a switch is pending: the task opens what is wanted now */
    }
    if (SRC_STORY == was)
    {
        s_story.part++;
        s_story.offset = 0;
        if (!open_story_part())
        {
            write_silence(DRAIN_MS);   /* let the DMA buffer play out */
            s_story.state = STORY_IDLE;
            ESP_LOGI(TAG, "story %03u done", (unsigned)s_story.id);
            post_event(AUDIO_EVT_STORY_DONE, s_story.id);
        }
        else
        {
            s_open     = SRC_STORY;
            s_open_gen = s_story.gen;
            s_gain     = s_gain_target;      /* seamless into the next part */
        }
    }
}

/* ---- Commands -------------------------------------------------------- */

static void handle(const cmd_t *p_cmd)
{
    switch (p_cmd->type)
    {
        case CMD_PLAY:
            s_story = (story_t){ .state = STORY_PLAYING, .id = p_cmd->arg,
                                 .gen = s_story.gen + 1u };
            break;
        case CMD_PAUSE:
            if (STORY_PLAYING == s_story.state)
            {
                s_story.state = STORY_PAUSED;
            }
            break;
        case CMD_RESUME:
            if (STORY_PAUSED == s_story.state)
            {
                s_story.state = STORY_PLAYING;
                s_story.gen++;          /* reopen at the saved position */
            }
            break;
        case CMD_STOP:
            s_story.state = STORY_IDLE;
            s_story.gen++;              /* position no longer wanted */
            break;
        case CMD_CHIME:
            s_b_chime_req = true;
            break;
        case CMD_VOLUME:
            s_volume = (p_cmd->arg < AUDIO_VOLUME_STEPS) ? p_cmd->arg
                                                         : (uint8_t)(AUDIO_VOLUME_STEPS - 1u);
            break;
        case CMD_MUTE:
            s_b_mute = (0u != p_cmd->arg);
            break;
        default:
            break;
    }
}

/** Rewind the saved offset by AUDIO_REWIND_MS worth of bytes. */
static void apply_rewind(void)
{
    const long back = ((long)s_kbps * 1000L / 8L) * (long)AUDIO_REWIND_MS / 1000L;

    s_story.offset = (s_story.offset > back) ? (s_story.offset - back) : 0;
}

/* ---- Decode one frame ------------------------------------------------ */

static void play_frame(void)
{
    mp3dec_frame_info_t info;
    size_t              written;
    int                 n;

    if (!s_b_eof && (s_avail < REFILL_BELOW))
    {
        const int64_t t0 = esp_timer_get_time();
        int64_t       dt;

        (void)memmove(s_in, &s_in[s_pos], s_avail);
        s_buf_off += (long)s_pos;
        s_pos      = 0u;
        s_avail   += fread(&s_in[s_avail], 1u, IN_BUF_BYTES - s_avail, s_f);
        s_b_eof    = (0 != feof(s_f)) || (0 != ferror(s_f));
        dt         = esp_timer_get_time() - t0;
        s_max_read_us = (dt > s_max_read_us) ? dt : s_max_read_us;
    }
    if (0u == s_avail)
    {
        source_finished();
        return;
    }

    n = mp3dec_decode_frame(&s_dec, &s_in[s_pos], (int)s_avail, s_pcm, &info);
    if (0 == info.frame_bytes)
    {
        source_finished();              /* no more frames in the data */
        return;
    }
    s_pos   += (size_t)info.frame_bytes;
    s_avail -= (size_t)info.frame_bytes;
    if (0 == n)
    {
        return;                         /* ID3 tag or junk skipped */
    }
    if (s_b_first)
    {
        i2s_set_rate((uint32_t)info.hz);
        s_b_first = false;
    }
    s_kbps = info.bitrate_kbps;

    for (int i = 0; i < n; i++)
    {
        int32_t s = (1 == info.channels) ? s_pcm[i]
                  : (((int32_t)s_pcm[2 * i] + (int32_t)s_pcm[2 * i + 1]) / 2);

        /* gain ramp: one step per sample towards the target */
        if (s_gain < s_gain_target)
        {
            s_gain = ((s_gain_target - s_gain) > s_ramp_step) ? (s_gain + s_ramp_step)
                                                              : s_gain_target;
        }
        else if (s_gain > s_gain_target)
        {
            s_gain = ((s_gain - s_gain_target) > s_ramp_step) ? (s_gain - s_ramp_step)
                                                              : s_gain_target;
        }
        else
        {
            /* at target */
        }
        s = (int32_t)(((int64_t)s * s_gain) >> 16);
        s = (s > INT16_MAX) ? INT16_MAX : ((s < INT16_MIN) ? INT16_MIN : s);
        s_out[2 * i]     = (int16_t)s;
        s_out[2 * i + 1] = (int16_t)s;
    }
    (void)i2s_channel_write(s_tx, s_out, (size_t)n * 2u * sizeof(int16_t),
                            &written, portMAX_DELAY);
    if (s_b_snap)
    {
        s_underruns_at_open = s_underruns;
        s_b_snap            = false;
    }
}

/* ---- Task ------------------------------------------------------------ */

static src_t wanted(void)
{
    if (s_b_chime_req || (SRC_CHIME == s_open))
    {
        return SRC_CHIME;
    }
    return (STORY_PLAYING == s_story.state) ? SRC_STORY : SRC_NONE;
}

static void audio_task(void *p_arg)
{
    cmd_t cmd;

    (void)p_arg;

    for (;;)
    {
        src_t want;

        /* Nothing open and nothing wanted: sleep until a command */
        if ((SRC_NONE == s_open) && (SRC_NONE == wanted()))
        {
            (void)xQueueReceive(s_cmds, &cmd, portMAX_DELAY);
            handle(&cmd);
        }
        while (pdTRUE == xQueueReceive(s_cmds, &cmd, 0))
        {
            handle(&cmd);
        }

        want = wanted();
        if ((s_open != want) ||
            ((SRC_STORY == s_open) && (s_open_gen != s_story.gen)))
        {
            /* Switch: fade out first, then close and open */
            if ((SRC_STORY == s_open) && (s_gain > 0))
            {
                s_gain_target = 0;
                play_frame();
                continue;
            }
            if (SRC_NONE != s_open)
            {
                const bool b_resume_later = (SRC_STORY == s_open);

                close_source();
                if (b_resume_later && (STORY_IDLE != s_story.state))
                {
                    apply_rewind();
                }
            }
            open_wanted(want);
        }

        s_gain_target = volume_gain();
        if (SRC_NONE != s_open)
        {
            play_frame();
        }
    }
}

/* ---- Public API ------------------------------------------------------ */

esp_err_t audio_init(const audio_config_t *p_cfg)
{
    gpio_config_t sd_cfg = { 0 };

    ESP_RETURN_ON_FALSE(NULL != p_cfg, ESP_ERR_INVALID_ARG, TAG, "no config");
    ESP_RETURN_ON_FALSE(NULL == s_cmds, ESP_ERR_INVALID_STATE, TAG, "already started");
    s_cfg    = *p_cfg;
    s_volume = (s_cfg.start_volume < AUDIO_VOLUME_STEPS) ? s_cfg.start_volume
                                                         : (uint8_t)(AUDIO_VOLUME_STEPS - 1u);

    /* 1. amp off, 2. I2S with silence, 3. amp on (bring-up 08, C3) */
    sd_cfg.pin_bit_mask = 1ULL << s_cfg.amp_sd;
    sd_cfg.mode         = GPIO_MODE_OUTPUT;
    ESP_RETURN_ON_ERROR(gpio_config(&sd_cfg), TAG, "amp pin");
    amp_enable(false);
    ESP_RETURN_ON_ERROR(i2s_start(), TAG, "i2s");
    write_silence(200u);
    amp_enable(true);

    s_cmds = xQueueCreate(CMD_DEPTH, sizeof(cmd_t));
    s_evts = xQueueCreate(EVT_DEPTH, sizeof(audio_event_t));
    ESP_RETURN_ON_FALSE((NULL != s_cmds) && (NULL != s_evts), ESP_ERR_NO_MEM, TAG, "queues");
    ESP_RETURN_ON_FALSE(pdPASS == xTaskCreatePinnedToCore(audio_task, "audio", TASK_STACK,
                                                          NULL, s_cfg.task_prio, NULL,
                                                          s_cfg.core),
                        ESP_ERR_NO_MEM, TAG, "task");
    return ESP_OK;
}

static void post(cmd_type_t type, uint8_t arg)
{
    const cmd_t cmd = { .type = type, .arg = arg };

    if ((NULL == s_cmds) || (pdTRUE != xQueueSend(s_cmds, &cmd, 0)))
    {
        ESP_LOGW(TAG, "command %d dropped", (int)type);
    }
}

void audio_play_story(uint8_t story_id) { post(CMD_PLAY, story_id); }
void audio_pause(void)                  { post(CMD_PAUSE, 0u); }
void audio_resume(void)                 { post(CMD_RESUME, 0u); }
void audio_stop(void)                   { post(CMD_STOP, 0u); }
void audio_play_chime(void)             { post(CMD_CHIME, 0u); }
void audio_set_volume(uint8_t step)     { post(CMD_VOLUME, step); }
void audio_set_mute(bool b_mute)        { post(CMD_MUTE, b_mute ? 1u : 0u); }

bool audio_get_event(audio_event_t *p_evt, uint32_t timeout_ms)
{
    if ((NULL == s_evts) || (NULL == p_evt))
    {
        return false;
    }
    return pdTRUE == xQueueReceive(s_evts, p_evt, pdMS_TO_TICKS(timeout_ms));
}
