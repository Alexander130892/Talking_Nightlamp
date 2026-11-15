/**
 * @file  19_cards.c
 * @brief Card writer: give blank cards a story number, using the lamp's
 *        own knob and button (no serial typing needed).
 *
 *   turn the knob    choose the story number (shown in the log)
 *   big button       write it to the card on the reader, verify, then
 *                    step to the next number (so 1, 2, 3 ... go quickly)
 *   press the knob   read the card on the reader and show what it holds
 *
 * Card record: docs/firmware/card-format.md ('S' 'B' 01 story hue in block 4).
 * The hue is a fixed colour per story, ready for when the tint (F15) returns.
 * Before writing, the old content is logged, so an overwrite is never silent.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "input.h"
#include "rc522.h"
#include "tag_format.h"

#define STORY_MIN (1)
#define STORY_MAX (255)

static const char *TAG = "cards";

static void uid_str(const rc522_tag_t *p_tag, char *p_s, size_t len)
{
    size_t n = 0u;

    for (uint8_t i = 0u; (i < p_tag->uid_len) && ((n + 3u) < len); i++)
    {
        n += (size_t)snprintf(&p_s[n], len - n, "%02X", p_tag->uid[i]);
    }
}

/** Poll and read the card on the reader. false: no card or unreadable. */
static bool read_card(rc522_tag_t *p_tag, tag_record_t *p_rec, bool *p_b_ours)
{
    uint8_t   buf[RC522_RECORD_LEN];
    esp_err_t err = rc522_poll(p_tag);

    if (ESP_OK != err)
    {
        ESP_LOGW(TAG, "no card on the reader (%s)", esp_err_to_name(err));
        return false;
    }
    err = rc522_read_record(p_tag, buf);
    if (ESP_OK != err)
    {
        ESP_LOGW(TAG, "card found but not readable (%s)", esp_err_to_name(err));
        return false;
    }
    *p_b_ours = (TAG_OK == tag_parse(buf, sizeof(buf), p_rec));
    return true;
}

static void show_card(void)
{
    rc522_tag_t  tag;
    tag_record_t rec = { 0 };
    bool         b_ours = false;
    char         uid[24] = "";

    if (read_card(&tag, &rec, &b_ours))
    {
        uid_str(&tag, uid, sizeof(uid));
        if (b_ours)
        {
            ESP_LOGI(TAG, "card %s holds story %03u (hue %u)", uid,
                     (unsigned)rec.story_id, (unsigned)rec.hue);
        }
        else
        {
            ESP_LOGI(TAG, "card %s is blank / not one of ours", uid);
        }
    }
}

static bool write_card(int story)
{
    const tag_record_t rec = { .story_id = (uint8_t)story,
                               .hue      = (uint8_t)((story * 37) & 0xFF) };
    uint8_t      buf[RC522_RECORD_LEN] = { 0 };
    rc522_tag_t  tag;
    tag_record_t old = { 0 };
    bool         b_ours = false;
    char         uid[24] = "";
    esp_err_t    err;

    if (!read_card(&tag, &old, &b_ours))
    {
        return false;
    }
    uid_str(&tag, uid, sizeof(uid));
    if (b_ours)
    {
        ESP_LOGI(TAG, "card %s was story %03u, overwriting", uid, (unsigned)old.story_id);
    }
    if (TAG_OK != tag_build(&rec, buf, sizeof(buf)))
    {
        ESP_LOGE(TAG, "cannot build record for story %d", story);
        return false;
    }
    err = rc522_write_record(&tag, buf);     /* writes and verifies */
    if (ESP_OK != err)
    {
        ESP_LOGE(TAG, "write FAILED on %s: %s (keep the card still, try again)",
                 uid, esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG, "card %s is now story %03u  -- OK, label it %d", uid,
             (unsigned)story, story);
    return true;
}

void app_main(void)
{
    const rc522_config_t rc_cfg =
    {
        .host = SPI3_HOST, .sck = GPIO_NUM_18, .miso = GPIO_NUM_19,
        .mosi = GPIO_NUM_23, .cs = GPIO_NUM_5, .rst = GPIO_NUM_22,
        .clock_hz = 1000000,
    };
    const input_config_t in_cfg =
    {
        .lamp_btn = GPIO_NUM_16, .enc_sw = GPIO_NUM_17, .enc_a = GPIO_NUM_34,
        .enc_b = GPIO_NUM_35, .counts_per_step = 4, .task_prio = 5u,
    };
    int           story = STORY_MIN;
    input_event_t evt;

    ESP_ERROR_CHECK(rc522_init(&rc_cfg));
    ESP_ERROR_CHECK(input_init(&in_cfg));
    ESP_LOGI(TAG, "knob = choose, big button = write, knob press = read");
    ESP_LOGI(TAG, "next story: %03d", story);

    for (;;)
    {
        if (!input_get_event(&evt, 1000u))
        {
            continue;
        }
        switch (evt.type)
        {
            case INPUT_EVT_ENC_STEP:
                story += evt.steps;
                story  = (story < STORY_MIN) ? STORY_MIN :
                         ((story > STORY_MAX) ? STORY_MAX : story);
                ESP_LOGI(TAG, "next story: %03d", story);
                break;
            case INPUT_EVT_LAMP_PRESS:
                if (write_card(story) && (story < STORY_MAX))
                {
                    story++;
                    ESP_LOGI(TAG, "next story: %03d (lay the next card)", story);
                }
                break;
            case INPUT_EVT_ENC_PRESS:
                show_card();
                break;
            default:
                break;
        }
    }
}
