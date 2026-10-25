/**
 * @file  storage.c
 * @brief microSD storage. TODO: implement.
 */

#include "storage.h"

esp_err_t storage_init(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t storage_story_exists(uint8_t story_id)
{
    (void)story_id;

    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t storage_story_part_path(uint8_t story_id, uint8_t part,
                                  char *p_path, size_t len)
{
    (void)story_id;
    (void)part;
    (void)p_path;
    (void)len;

    return ESP_ERR_NOT_SUPPORTED;
}
