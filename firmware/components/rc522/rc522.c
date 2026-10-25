/**
 * @file  rc522.c
 * @brief MFRC522 driver. TODO: implement.
 */

#include "rc522.h"

esp_err_t rc522_init(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t rc522_poll(rc522_tag_t *p_tag)
{
    (void)p_tag;

    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t rc522_read_record(const rc522_tag_t *p_tag, uint8_t p_buf[16])
{
    (void)p_tag;
    (void)p_buf;

    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t rc522_write_record(const rc522_tag_t *p_tag, const uint8_t p_buf[16])
{
    (void)p_tag;
    (void)p_buf;

    return ESP_ERR_NOT_SUPPORTED;
}
