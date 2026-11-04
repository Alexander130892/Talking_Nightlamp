/**
 * @file  storage.c
 * @brief SD card mount and story lookup (see storage.h for the contract).
 */

#include "storage.h"

#include <ctype.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver/sdspi_host.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#define STORIES_DIR  STORAGE_MOUNT_POINT "/stories"
#define NO_STORY     (-1)

static const char *TAG = "storage";
static bool        s_b_mounted;

/* Part list cache for one story */
static int      s_cached_id = NO_STORY;
static uint32_t s_n_parts;
static char     s_names[STORAGE_MAX_PARTS][STORAGE_MAX_NAME];

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

/** Read and sort the parts of story_id into the cache (if not cached). */
static void load_story(uint8_t story_id)
{
    char  dir[32];
    DIR  *p_d;

    if ((int)story_id == s_cached_id)
    {
        return;
    }
    s_cached_id = (int)story_id;
    s_n_parts   = 0u;

    (void)snprintf(dir, sizeof(dir), STORIES_DIR "/%03u", (unsigned)story_id);
    p_d = opendir(dir);
    if (NULL == p_d)
    {
        return;   /* no such story: 0 parts */
    }
    for (struct dirent *p_e = readdir(p_d); NULL != p_e; p_e = readdir(p_d))
    {
        if ((DT_REG != p_e->d_type) || !is_mp3(p_e->d_name))
        {
            continue;   /* folders, ._ files, non-MP3 */
        }
        if (strlen(p_e->d_name) >= STORAGE_MAX_NAME)
        {
            ESP_LOGW(TAG, "%s: name too long, skipped: %s", dir, p_e->d_name);
            continue;
        }
        if (s_n_parts >= STORAGE_MAX_PARTS)
        {
            ESP_LOGW(TAG, "%s: more than %u parts, rest ignored", dir,
                     (unsigned)STORAGE_MAX_PARTS);
            break;
        }
        (void)strcpy(s_names[s_n_parts], p_e->d_name);
        s_n_parts++;
    }
    (void)closedir(p_d);
    qsort(s_names, s_n_parts, STORAGE_MAX_NAME, cmp_name);
}

esp_err_t storage_init(const storage_config_t *p_cfg)
{
    const esp_vfs_fat_sdmmc_mount_config_t mount_cfg =
    {
        .format_if_mount_failed = false,   /* never: it would erase stories */
        .max_files              = 2,
        .allocation_unit_size   = 0,
    };
    spi_bus_config_t      bus  = { 0 };
    sdmmc_host_t          host = SDSPI_HOST_DEFAULT();
    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    sdmmc_card_t         *p_card;
    esp_err_t             err;

    ESP_RETURN_ON_FALSE(NULL != p_cfg, ESP_ERR_INVALID_ARG, TAG, "no config");
    ESP_RETURN_ON_FALSE(!s_b_mounted, ESP_ERR_INVALID_STATE, TAG, "already mounted");

    bus.mosi_io_num     = p_cfg->mosi;
    bus.miso_io_num     = p_cfg->miso;
    bus.sclk_io_num     = p_cfg->sck;
    bus.quadwp_io_num   = -1;
    bus.quadhd_io_num   = -1;
    bus.max_transfer_sz = 4000;
    host.slot           = p_cfg->host;
    host.max_freq_khz   = p_cfg->freq_khz;
    slot.host_id        = p_cfg->host;
    slot.gpio_cs        = p_cfg->cs;

    ESP_RETURN_ON_ERROR(spi_bus_initialize(p_cfg->host, &bus, SPI_DMA_CH_AUTO),
                        TAG, "spi bus");
    err = esp_vfs_fat_sdspi_mount(STORAGE_MOUNT_POINT, &host, &slot, &mount_cfg,
                                  &p_card);
    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "mount failed: %s", esp_err_to_name(err));
        (void)spi_bus_free(p_cfg->host);
        return err;
    }
    s_b_mounted = true;
    ESP_LOGI(TAG, "mounted, %llu MB", ((unsigned long long)p_card->csd.capacity *
                                       p_card->csd.sector_size) / (1024u * 1024u));
    return ESP_OK;
}

esp_err_t storage_story_exists(uint8_t story_id)
{
    ESP_RETURN_ON_FALSE(s_b_mounted, ESP_ERR_INVALID_STATE, TAG, "not mounted");
    load_story(story_id);
    return (0u != s_n_parts) ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t storage_story_part_path(uint8_t story_id, uint8_t part,
                                  char *p_path, size_t len)
{
    int n;

    ESP_RETURN_ON_FALSE(s_b_mounted, ESP_ERR_INVALID_STATE, TAG, "not mounted");
    ESP_RETURN_ON_FALSE(NULL != p_path, ESP_ERR_INVALID_ARG, TAG, "no buffer");
    load_story(story_id);
    if (part >= s_n_parts)
    {
        return ESP_ERR_NOT_FOUND;
    }
    n = snprintf(p_path, len, STORIES_DIR "/%03u/%s", (unsigned)story_id,
                 s_names[part]);
    return ((n > 0) && ((size_t)n < len)) ? ESP_OK : ESP_ERR_INVALID_SIZE;
}
