/**
 * @file  08b_mp3.c
 * @brief Bring-up step 8b: MP3 from SD -> minimp3 -> I2S -> MAX98357A.
 *
 * Plays /sd/system/unknown.mp3, then every MP3 in /sd/stories/007/ in name
 * order (skipping names starting with '.'), then repeats.
 * Per file it logs format, duration and decoder CPU load.
 *
 * Runs in its own task: mp3dec_decode_frame() needs ~16 KB of stack.
 */

#include <ctype.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "minimp3.h"

/* Pins (docs/hardware/pin-mapping.md) */
#define PIN_SD_SCK       (14)
#define PIN_SD_MISO      (27)
#define PIN_SD_MOSI      (13)
#define PIN_SD_CS        (4)
#define PIN_I2S_BCLK     (26)
#define PIN_I2S_LRC      (25)
#define PIN_I2S_DIN      (33)
#define PIN_AMP_SD       (21)

#define MOUNT_POINT      "/sd"
#define CHIME_FILE       MOUNT_POINT "/system/unknown.mp3"
#define STORY_DIR        MOUNT_POINT "/stories/007"

#define VOLUME_Q15       (16384)   /* 0.5 = -6 dB software volume */
#define IN_BUF_BYTES     (8192u)
#define REFILL_BELOW     (2048u)   /* > largest MP3 frame (1441 B) */
#define MAX_FILES        (16u)
#define MAX_NAME         (64u)
#define PLAYER_STACK     (32u * 1024u)

static const char *TAG = "mp3";

static i2s_chan_handle_t s_tx;
static uint32_t          s_rate_hz;

static mp3dec_t s_dec;                                   /* ~6.7 KB */
static uint8_t  s_in[IN_BUF_BYTES];
static int16_t  s_pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
static int16_t  s_out[MINIMP3_MAX_SAMPLES_PER_FRAME];    /* stereo frames */

/* ---- Amplifier and I2S --------------------------------------------- */

static void amp_enable(bool b_on)
{
    ESP_ERROR_CHECK(gpio_set_level(PIN_AMP_SD, b_on ? 1 : 0));
}

static void write_silence(uint32_t ms)
{
    const uint32_t frames = (s_rate_hz * ms) / 1000u;
    size_t         written;

    (void)memset(s_out, 0, sizeof(s_out));
    for (uint32_t done = 0u; done < frames; done += (sizeof(s_out) / 4u))
    {
        ESP_ERROR_CHECK(i2s_channel_write(s_tx, s_out, sizeof(s_out),
                                          &written, portMAX_DELAY));
    }
}

static void i2s_init(uint32_t rate_hz)
{
    i2s_chan_config_t chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    const i2s_std_config_t std_cfg =
    {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(rate_hz),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                        I2S_SLOT_MODE_STEREO),
        .gpio_cfg =
        {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = PIN_I2S_BCLK,
            .ws   = PIN_I2S_LRC,
            .dout = PIN_I2S_DIN,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = { false, false, false },
        },
    };

    /* On underrun send zeros, instead of repeating the last DMA buffer
     * (which sounds like a buzz between files). */
    chan_cfg.auto_clear = true;

    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &s_tx, NULL));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_tx, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(s_tx));
    s_rate_hz = rate_hz;
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
    ESP_ERROR_CHECK(i2s_channel_disable(s_tx));
    ESP_ERROR_CHECK(i2s_channel_reconfig_std_clock(s_tx, &clk));
    ESP_ERROR_CHECK(i2s_channel_enable(s_tx));
    s_rate_hz = rate_hz;
    write_silence(50u);
    amp_enable(true);
    ESP_LOGI(TAG, "I2S rate -> %lu Hz", (unsigned long)rate_hz);
}

/* ---- SD card ------------------------------------------------------- */

static bool sd_mount(void)
{
    const esp_vfs_fat_sdmmc_mount_config_t mount_cfg =
    {
        .format_if_mount_failed = false,
        .max_files              = 4,
        .allocation_unit_size   = 16u * 1024u,
    };
    const spi_bus_config_t bus =
    {
        .mosi_io_num     = PIN_SD_MOSI,
        .miso_io_num     = PIN_SD_MISO,
        .sclk_io_num     = PIN_SD_SCK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 4000,
    };
    sdmmc_host_t          host = SDSPI_HOST_DEFAULT();
    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    sdmmc_card_t         *p_card;
    esp_err_t             err;

    host.slot         = SPI2_HOST;
    host.max_freq_khz = 10000;
    slot.host_id      = SPI2_HOST;
    slot.gpio_cs      = PIN_SD_CS;

    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
    err = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot, &mount_cfg,
                                  &p_card);
    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "SD mount failed: %s", esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG, "SD mounted");
    return true;
}

static bool is_mp3(const char *p_name)
{
    const size_t len = strlen(p_name);

    return ('.' != p_name[0]) && (len > 4u) &&
           ('.' == p_name[len - 4u]) &&
           ('m' == tolower((unsigned char)p_name[len - 3u])) &&
           ('p' == tolower((unsigned char)p_name[len - 2u])) &&
           ('3' == p_name[len - 1u]);
}

static int cmp_name(const void *p_a, const void *p_b)
{
    return strcmp((const char *)p_a, (const char *)p_b);
}

/** MP3 files in p_dir, name order, hidden ('.') files skipped. */
static uint32_t list_mp3(const char *p_dir, char names[][MAX_NAME])
{
    DIR     *p_d = opendir(p_dir);
    uint32_t n   = 0u;

    if (NULL == p_d)
    {
        ESP_LOGW(TAG, "%s: not found", p_dir);
        return 0u;
    }
    for (struct dirent *p_e = readdir(p_d); (NULL != p_e) && (n < MAX_FILES);
         p_e = readdir(p_d))
    {
        if ((DT_REG == p_e->d_type) && is_mp3(p_e->d_name) &&
            (strlen(p_e->d_name) < MAX_NAME))
        {
            (void)strcpy(names[n], p_e->d_name);
            n++;
        }
        else if ('.' == p_e->d_name[0])
        {
            ESP_LOGI(TAG, "skip hidden %s", p_e->d_name);
        }
        else
        {
            /* not an MP3 */
        }
    }
    (void)closedir(p_d);
    qsort(names, n, MAX_NAME, cmp_name);
    return n;
}

/* ---- Decode and play one file -------------------------------------- */

static void play_file(const char *p_path)
{
    FILE               *p_f = fopen(p_path, "rb");
    mp3dec_frame_info_t info;
    size_t              avail     = 0u;
    size_t              pos       = 0u;
    bool                b_eof     = false;
    bool                b_first   = true;
    uint32_t            frames    = 0u;
    uint64_t            samples   = 0u;
    int64_t             t_decode  = 0;
    const int64_t       t_start   = esp_timer_get_time();

    if (NULL == p_f)
    {
        ESP_LOGE(TAG, "cannot open %s", p_path);
        return;
    }
    ESP_LOGI(TAG, "play %s", p_path);
    mp3dec_init(&s_dec);

    for (;;)
    {
        int     n;
        int64_t t0;

        /* Keep at least one full frame in the input buffer */
        if (!b_eof && (avail < REFILL_BELOW))
        {
            (void)memmove(s_in, &s_in[pos], avail);
            pos    = 0u;
            avail += fread(&s_in[avail], 1u, IN_BUF_BYTES - avail, p_f);
            b_eof  = feof(p_f);
        }
        if (0u == avail)
        {
            break;
        }

        t0 = esp_timer_get_time();
        n  = mp3dec_decode_frame(&s_dec, &s_in[pos], (int)avail, s_pcm, &info);
        t_decode += esp_timer_get_time() - t0;

        if (0 == info.frame_bytes)
        {
            break;              /* no more frames in the remaining data */
        }
        pos   += (size_t)info.frame_bytes;
        avail -= (size_t)info.frame_bytes;
        if (0 == n)
        {
            continue;           /* skipped ID3 tag or junk */
        }

        if (b_first)
        {
            ESP_LOGI(TAG, "  %d Hz, %s, %d kbit/s", info.hz,
                     (1 == info.channels) ? "mono" : "stereo",
                     info.bitrate_kbps);
            i2s_set_rate((uint32_t)info.hz);
            b_first = false;
        }

        /* To stereo I2S frames: mono duplicated, stereo downmixed
         * (the amp plays one channel), then software volume. */
        for (int i = 0; i < n; i++)
        {
            int32_t s;

            if (1 == info.channels)
            {
                s = s_pcm[i];
            }
            else
            {
                s = ((int32_t)s_pcm[2 * i] + (int32_t)s_pcm[2 * i + 1]) / 2;
            }
            s = (s * VOLUME_Q15) >> 15;
            s_out[2 * i]     = (int16_t)s;
            s_out[2 * i + 1] = (int16_t)s;
        }

        {
            size_t written;

            ESP_ERROR_CHECK(i2s_channel_write(s_tx, s_out,
                                              (size_t)n * 2u * sizeof(int16_t),
                                              &written, portMAX_DELAY));
        }
        frames++;
        samples += (uint64_t)n;
    }
    (void)fclose(p_f);

    if (0u != frames)
    {
        const uint32_t audio_ms = (uint32_t)((samples * 1000u) / s_rate_hz);
        const uint32_t wall_ms  =
            (uint32_t)((esp_timer_get_time() - t_start) / 1000);

        /* decoder us per audio ms = permille of one core */
        const uint32_t cpu_pm = (uint32_t)(t_decode / (int64_t)audio_ms);

        ESP_LOGI(TAG, "  %lu frames, %lu ms audio in %lu ms, "
                 "decoder %lu ms = %lu.%lu %% of one core",
                 (unsigned long)frames, (unsigned long)audio_ms,
                 (unsigned long)wall_ms, (unsigned long)(t_decode / 1000),
                 (unsigned long)(cpu_pm / 10u), (unsigned long)(cpu_pm % 10u));
    }
    else
    {
        ESP_LOGW(TAG, "  no MP3 frames found");
    }
}

/* ---- Player task --------------------------------------------------- */

static void player_task(void *p_arg)
{
    static char names[MAX_FILES][MAX_NAME];
    char        path[160];

    (void)p_arg;

    for (;;)
    {
        play_file(CHIME_FILE);
        write_silence(1000u);

        const uint32_t n = list_mp3(STORY_DIR, names);

        ESP_LOGI(TAG, "%s: %lu MP3 file(s)", STORY_DIR, (unsigned long)n);
        for (uint32_t i = 0u; i < n; i++)
        {
            (void)snprintf(path, sizeof(path), "%s/%s", STORY_DIR, names[i]);
            play_file(path);
        }
        write_silence(3000u);
    }
}

void app_main(void)
{
    const gpio_config_t sd_cfg =
    {
        .pin_bit_mask = (1ULL << PIN_AMP_SD),
        .mode         = GPIO_MODE_OUTPUT,
    };

    ESP_ERROR_CHECK(gpio_config(&sd_cfg));
    amp_enable(false);                 /* 1. amp off */

    i2s_init(44100u);                  /* 2. I2S running ... */
    write_silence(200u);               /*    ... with silence */
    amp_enable(true);                  /* 3. amp on */

    if (!sd_mount())
    {
        return;
    }

    (void)xTaskCreatePinnedToCore(player_task, "player", PLAYER_STACK, NULL,
                                  5, NULL, 1);
}
