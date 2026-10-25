/**
 * @file  audio.c
 * @brief Audio pipeline. TODO: implement.
 */

#include "audio.h"

esp_err_t audio_init(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t audio_play_story(uint8_t story_id)
{
    (void)story_id;

    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t audio_pause(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t audio_resume(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t audio_stop(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t audio_play_chime(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t audio_set_volume(uint8_t step)
{
    (void)step;

    return ESP_ERR_NOT_SUPPORTED;
}

bool audio_story_finished(void)
{
    return false;
}
