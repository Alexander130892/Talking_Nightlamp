/**
 * @file  input.c
 * @brief Button and encoder input. TODO: implement.
 */

#include "input.h"

esp_err_t input_init(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

bool input_get_event(input_event_t *p_evt, uint32_t timeout_ms)
{
    (void)p_evt;
    (void)timeout_ms;

    return false;
}
