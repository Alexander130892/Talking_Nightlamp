/**
 * @file  07_sd.c
 * @brief Bring-up step 7: microSD on SPI2 (HSPI), FAT mount.
 *
 * 1. Mount the card read/write at /sd and print card info
 * 2. List /sd and /sd/stories
 * 3. Write a 256 KiB test file, read it back, verify, report kB/s
 * 4. Delete the test file and unmount
 *
 * Wiring: docs/hardware/wiring.md (module VCC on 5 V = devkit VIN,
 * module modified per ADR 0012).
 */

#include <dirent.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#define PIN_SD_SCK       (14)
#define PIN_SD_MISO      (27)
#define PIN_SD_MOSI      (13)
#define PIN_SD_CS        (4)

#define SD_FREQ_KHZ      (10000)     /* 10 MHz: plenty for 12 kB/s of MP3 */
#define MOUNT_POINT      "/sd"
#define TEST_FILE        MOUNT_POINT "/bringup.bin"
#define TEST_BYTES       (256u * 1024u)
#define CHUNK_BYTES      (4096u)

static const char *TAG = "sd";
static uint8_t s_buf[CHUNK_BYTES];

/* Pattern that depends on the absolute offset: catches shifted or
 * repeated chunks, not only flipped bits. */
static void fill_pattern(uint8_t *p_buf, uint32_t offset)
{
    for (uint32_t i = 0u; i < CHUNK_BYTES; i++)
    {
        const uint32_t pos = offset + i;

        p_buf[i] = (uint8_t)((pos * 31u) ^ (pos >> 8));
    }
}

static void list_dir(const char *p_path)
{
    DIR *p_dir = opendir(p_path);

    if (NULL == p_dir)
    {
        ESP_LOGW(TAG, "%s: not found", p_path);
        return;
    }

    ESP_LOGI(TAG, "%s:", p_path);
    for (struct dirent *p_ent = readdir(p_dir); NULL != p_ent;
         p_ent = readdir(p_dir))
    {
        ESP_LOGI(TAG, "    %s%s", p_ent->d_name,
                 (DT_DIR == p_ent->d_type) ? "/" : "");
    }
    (void)closedir(p_dir);
}

static bool write_test(void)
{
    FILE   *p_f = fopen(TEST_FILE, "wb");
    int64_t t0;
    int64_t dt;

    if (NULL == p_f)
    {
        ESP_LOGE(TAG, "write: cannot create %s", TEST_FILE);
        return false;
    }

    t0 = esp_timer_get_time();
    for (uint32_t off = 0u; off < TEST_BYTES; off += CHUNK_BYTES)
    {
        fill_pattern(s_buf, off);
        if (CHUNK_BYTES != fwrite(s_buf, 1u, CHUNK_BYTES, p_f))
        {
            ESP_LOGE(TAG, "write: short write at %lu", (unsigned long)off);
            (void)fclose(p_f);
            return false;
        }
    }
    (void)fclose(p_f);   /* includes flushing to the card */
    dt = esp_timer_get_time() - t0;

    ESP_LOGI(TAG, "write: %u KiB in %lld ms = %lld kB/s",
             (unsigned)(TEST_BYTES / 1024u), dt / 1000,
             ((int64_t)TEST_BYTES * 1000) / (dt > 0 ? dt : 1));
    return true;
}

static bool read_test(void)
{
    static uint8_t expect[CHUNK_BYTES];
    FILE   *p_f = fopen(TEST_FILE, "rb");
    int64_t t0;
    int64_t dt;
    bool    b_ok = true;

    if (NULL == p_f)
    {
        ESP_LOGE(TAG, "read: cannot open %s", TEST_FILE);
        return false;
    }

    t0 = esp_timer_get_time();
    for (uint32_t off = 0u; (off < TEST_BYTES) && b_ok; off += CHUNK_BYTES)
    {
        if (CHUNK_BYTES != fread(s_buf, 1u, CHUNK_BYTES, p_f))
        {
            ESP_LOGE(TAG, "read: short read at %lu", (unsigned long)off);
            b_ok = false;
        }
        else
        {
            fill_pattern(expect, off);
            if (0 != memcmp(s_buf, expect, CHUNK_BYTES))
            {
                ESP_LOGE(TAG, "read: MISMATCH in chunk at %lu",
                         (unsigned long)off);
                b_ok = false;
            }
        }
    }
    dt = esp_timer_get_time() - t0;
    (void)fclose(p_f);

    if (b_ok)
    {
        ESP_LOGI(TAG, "read:  %u KiB in %lld ms = %lld kB/s, verified",
                 (unsigned)(TEST_BYTES / 1024u), dt / 1000,
                 ((int64_t)TEST_BYTES * 1000) / (dt > 0 ? dt : 1));
    }
    return b_ok;
}

void app_main(void)
{
    const esp_vfs_fat_sdmmc_mount_config_t mount_cfg =
    {
        .format_if_mount_failed = false,  /* never wipe someone's card */
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
    sdmmc_host_t           host = SDSPI_HOST_DEFAULT();
    sdspi_device_config_t  slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    sdmmc_card_t          *p_card = NULL;
    esp_err_t              err;

    host.slot         = SPI2_HOST;
    host.max_freq_khz = SD_FREQ_KHZ;
    slot.host_id      = SPI2_HOST;
    slot.gpio_cs      = PIN_SD_CS;

    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));

    err = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot, &mount_cfg,
                                  &p_card);
    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "mount failed: %s", esp_err_to_name(err));
        ESP_LOGE(TAG, "check: card inserted? FAT32? wiring/power (VCC on 5 V)?");
        return;
    }

    sdmmc_card_print_info(stdout, p_card);
    ESP_LOGI(TAG, "SPI clock: %d kHz", p_card->real_freq_khz);

    list_dir(MOUNT_POINT);
    list_dir(MOUNT_POINT "/stories");

    if (write_test() && read_test())
    {
        ESP_LOGI(TAG, "RESULT: PASS");
    }
    else
    {
        ESP_LOGE(TAG, "RESULT: FAIL");
    }

    (void)remove(TEST_FILE);
    ESP_ERROR_CHECK(esp_vfs_fat_sdcard_unmount(MOUNT_POINT, p_card));
    ESP_LOGI(TAG, "unmounted, safe to remove the card");
}
