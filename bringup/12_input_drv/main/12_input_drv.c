/**
 * @file  12_input_drv.c
 * @brief Driver test 12: the real input driver, read by a "slow app".
 *
 * Normal mode: events are read as they come.
 * Slow mode (press the lamp button to toggle): the app sleeps SLOW_MS
 * between reads. Spin the knob fast while it sleeps: steps must arrive
 * summed in one or a few events, none lost (queue-full policy, input.h).
 */

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "input.h"

#define SLOW_MS (2000u)

static const char *TAG = "test";

void app_main(void)
{
    const input_config_t cfg =
    {
        .lamp_btn        = GPIO_NUM_16,
        .enc_sw          = GPIO_NUM_17,
        .enc_a           = GPIO_NUM_34,
        .enc_b           = GPIO_NUM_35,
        .counts_per_step = 4,
        .task_prio       = 5,
    };
    input_event_t evt;
    bool          b_slow = false;
    int           total  = 0;

    ESP_ERROR_CHECK(input_init(&cfg));
    ESP_LOGI(TAG, "ready. Lamp button toggles slow mode (%u ms)", SLOW_MS);

    for (;;)
    {
        if (!input_get_event(&evt, 1000u))
        {
            continue;
        }
        switch (evt.type)
        {
            case INPUT_EVT_LAMP_PRESS:
                b_slow = !b_slow;
                ESP_LOGI(TAG, "lamp press -> %s mode", b_slow ? "SLOW" : "normal");
                break;
            case INPUT_EVT_ENC_PRESS:
                ESP_LOGI(TAG, "encoder press");
                break;
            case INPUT_EVT_ENC_STEP:
                total += evt.steps;
                ESP_LOGI(TAG, "steps %+4d   total %+5d", evt.steps, total);
                break;
            default:
                break;
        }
        if (b_slow)
        {
            vTaskDelay(pdMS_TO_TICKS(SLOW_MS));
        }
    }
}
