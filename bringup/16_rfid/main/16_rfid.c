/**
 * @file  16_rfid.c
 * @brief Driver test 16: RFID task + card_fsm, actions logged (no audio).
 *
 * This is the real decision chain minus audio/lamp: poll -> card_reader ->
 * card_fsm. Try: place, lift 1 s and back, lift 5 s, back, swap cards,
 * two cards at once, a foreign card (bank card, transit card), a half lift.
 * Only CHANGES of the poll result and every action are logged.
 */

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "card_fsm.h"
#include "rfid.h"

static const char *TAG = "test";

static const char *act_name(card_action_t a)
{
    static const char *names[] = { "NONE", "PLAY", "RESUME", "PAUSE", "STOP",
                                   "LAMP_LOST", "LAMP_RESTORE", "UNKNOWN" };
    return names[a];
}

static void describe(const card_poll_t *p, char *p_s, size_t len)
{
    int n;

    if (!p->b_present)
    {
        (void)snprintf(p_s, len, "no card");
        return;
    }
    if (0u == p->uid_len)
    {
        (void)snprintf(p_s, len, "COLLISION (two cards)");
        return;
    }
    n = snprintf(p_s, len, "UID ");
    for (uint8_t i = 0u; (i < p->uid_len) && (n > 0) && ((size_t)n < len); i++)
    {
        n += snprintf(&p_s[n], len - (size_t)n, "%02X", p->uid[i]);
    }
    if ((n > 0) && ((size_t)n < len))
    {
        if (p->b_valid)
        {
            (void)snprintf(&p_s[n], len - (size_t)n, "  story %03u hue %u",
                           p->story_id, p->hue);
        }
        else
        {
            (void)snprintf(&p_s[n], len - (size_t)n, "  NOT one of ours");
        }
    }
}

void app_main(void)
{
    const rfid_config_t cfg =
    {
        .rc522 =
        {
            .host = SPI3_HOST, .sck = GPIO_NUM_18, .miso = GPIO_NUM_19,
            .mosi = GPIO_NUM_23, .cs = GPIO_NUM_5, .rst = GPIO_NUM_22,
            .clock_hz = 1000000,
        },
        .task_prio = 6u,
    };
    const card_fsm_cfg_t fsm_cfg = CARD_FSM_CFG_DEFAULT;
    card_fsm_t  fsm;
    card_poll_t poll;
    card_poll_t prev = { 0 };
    char        s[64];

    ESP_ERROR_CHECK(rfid_start(&cfg));
    card_fsm_init(&fsm, &fsm_cfg);
    ESP_LOGI(TAG, "ready");

    for (;;)
    {
        if (!rfid_get_poll(&poll, 1000u))
        {
            continue;
        }
        if (0 != memcmp(&poll, &prev, sizeof(poll)))
        {
            describe(&poll, s, sizeof(s));
            ESP_LOGI(TAG, "poll: %s", s);
            prev = poll;
        }

        {
            const card_output_t out =
                card_fsm_step(&fsm, &poll, (uint32_t)(esp_timer_get_time() / 1000));

            if (CARD_ACT_NONE != out.action)
            {
                ESP_LOGI(TAG, "   -> %s%s", act_name(out.action),
                         (CARD_ACT_PLAY == out.action) ? " (new story)" : "");
            }
        }
    }
}
