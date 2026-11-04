/**
 * @file  rfid.c
 * @brief RFID task (see rfid.h).
 */

#include "rfid.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"
#include "card_reader.h"

#define TASK_STACK (4096u)

static const char   *TAG = "rfid";
static QueueHandle_t s_polls;

/** card_reader callback: read the record of the tag just polled. */
static bool read_record(void *p_ctx, uint8_t p_buf[CARD_READER_BLOCK_LEN])
{
    const esp_err_t err = rc522_read_record((const rc522_tag_t *)p_ctx, p_buf);

    if (ESP_OK != err)
    {
        ESP_LOGW(TAG, "read failed: %s", esp_err_to_name(err));
    }
    return ESP_OK == err;
}

static card_rd_t classify(esp_err_t err)
{
    switch (err)
    {
        case ESP_OK:                   return CARD_RD_TAG;
        case ESP_ERR_NOT_FOUND:        return CARD_RD_NONE;
        case ESP_ERR_INVALID_RESPONSE: return CARD_RD_COLLISION;
        default:                       return CARD_RD_GARBLED;
    }
}

static void rfid_task(void *p_arg)
{
    card_reader_t reader;
    rc522_tag_t   tag;
    TickType_t    wake = xTaskGetTickCount();

    (void)p_arg;
    card_reader_init(&reader);

    for (;;)
    {
        const card_rd_t   st   = classify(rc522_poll(&tag));
        const card_poll_t poll = card_reader_step(&reader, st, tag.uid, tag.uid_len,
                                                  read_record, &tag);

        (void)xQueueOverwrite(s_polls, &poll);
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(RFID_PERIOD_MS));
    }
}

esp_err_t rfid_start(const rfid_config_t *p_cfg)
{
    ESP_RETURN_ON_FALSE(NULL != p_cfg, ESP_ERR_INVALID_ARG, TAG, "no config");
    ESP_RETURN_ON_FALSE(NULL == s_polls, ESP_ERR_INVALID_STATE, TAG, "already started");
    ESP_RETURN_ON_ERROR(rc522_init(&p_cfg->rc522), TAG, "rc522");

    s_polls = xQueueCreate(1u, sizeof(card_poll_t));
    ESP_RETURN_ON_FALSE(NULL != s_polls, ESP_ERR_NO_MEM, TAG, "queue");
    ESP_RETURN_ON_FALSE(pdPASS == xTaskCreate(rfid_task, "rfid", TASK_STACK, NULL,
                                              p_cfg->task_prio, NULL),
                        ESP_ERR_NO_MEM, TAG, "task");
    return ESP_OK;
}

bool rfid_get_poll(card_poll_t *p_poll, uint32_t timeout_ms)
{
    if ((NULL == s_polls) || (NULL == p_poll))
    {
        return false;
    }
    return pdTRUE == xQueueReceive(s_polls, p_poll, pdMS_TO_TICKS(timeout_ms));
}
