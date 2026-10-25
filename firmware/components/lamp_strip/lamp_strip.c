/**
 * @file  lamp_strip.c
 * @brief SK6812 RMT driver. TODO: implement.
 */

#include "lamp_strip.h"

esp_err_t lamp_strip_init(size_t n_pixels)
{
    (void)n_pixels;

    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t lamp_strip_show(const pixel_grbw_t *p_frame, size_t n)
{
    (void)p_frame;
    (void)n;

    return ESP_ERR_NOT_SUPPORTED;
}
