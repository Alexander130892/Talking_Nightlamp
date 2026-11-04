/**
 * @file  14_storage.c
 * @brief Driver test 14: mount, list every story 000..255 with its parts.
 *
 * Run twice: with the card in (expect your stories), and with the card
 * pulled (expect a clean error, no crash, no format).
 */

#include <stdio.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "storage.h"

static const char *TAG = "test";

void app_main(void)
{
    const storage_config_t cfg =
    {
        .host     = SPI2_HOST,
        .sck      = GPIO_NUM_14,
        .miso     = GPIO_NUM_27,
        .mosi     = GPIO_NUM_13,
        .cs       = GPIO_NUM_4,
        .freq_khz = 10000,
    };
    char      path[STORAGE_PATH_LEN];
    uint32_t  stories = 0u;
    int64_t   t0;
    esp_err_t err = storage_init(&cfg);

    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "storage_init: %s -> app would show the fault pattern",
                 esp_err_to_name(err));
        return;
    }

    t0 = esp_timer_get_time();
    for (uint32_t id = 0u; id < 256u; id++)
    {
        if (ESP_OK != storage_story_exists((uint8_t)id))
        {
            continue;
        }
        stories++;
        for (uint8_t part = 0u;
             ESP_OK == storage_story_part_path((uint8_t)id, part, path, sizeof(path));
             part++)
        {
            ESP_LOGI(TAG, "story %03lu part %u: %s", (unsigned long)id,
                     (unsigned)part, path);
        }
    }
    ESP_LOGI(TAG, "%lu stories, scan of 256 ids took %lld ms",
             (unsigned long)stories, (esp_timer_get_time() - t0) / 1000);

    /* Cache check: same story again must not touch the card */
    t0 = esp_timer_get_time();
    (void)storage_story_part_path(7u, 0u, path, sizeof(path));
    ESP_LOGI(TAG, "story 7 after scanning others (reload): %lld us", esp_timer_get_time() - t0);
    t0 = esp_timer_get_time();
    (void)storage_story_part_path(7u, 0u, path, sizeof(path));
    ESP_LOGI(TAG, "story 7 again (cached): %lld us", esp_timer_get_time() - t0);
}
