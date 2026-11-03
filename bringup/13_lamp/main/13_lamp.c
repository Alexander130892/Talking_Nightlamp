/**
 * @file  13_lamp.c
 * @brief Driver test 13: lamp task with fades, tint, pulse and breathing.
 *
 * Watch for: no jumps, no flicker, smooth start/end of every effect.
 * Every step is logged before it starts. Run the glow part in a DARK room:
 * one LED at W = 1 is invisible in normal room light.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lamp.h"
#include "lamp_tint.h"

#define GLOW_PM (80u)   /* lowest level with all 12 LEDs at W = 1 */

static const char *TAG = "test";

static void step(const char *p_what, uint32_t wait_ms)
{
    ESP_LOGI(TAG, "%s", p_what);
    vTaskDelay(pdMS_TO_TICKS(wait_ms));
}

void app_main(void)
{
    const lamp_config_t cfg =
    {
        .strip     = { .data_gpio = GPIO_NUM_32, .n_pixels = 12u },
        .task_prio = 3u,
    };

    ESP_ERROR_CHECK(lamp_start(&cfg));

    for (;;)
    {
        lamp_fade_to(1000u, 3000u);
        step("1. warm up 0 -> 100 % in 3 s (F1)", 4000u);

        lamp_pulse();
        step("2. pulse at full: soft dip (F9)", 2000u);

        lamp_breathe(true);
        step("3. breathing on (F2 grace signal)", 5000u);
        lamp_breathe(false);
        step("   breathing off: current dip finishes", 3000u);

        for (uint32_t h = 0u; h < 256u; h += 32u)
        {
            static char s_msg[48];

            lamp_set_tint((uint8_t)h, LAMP_TINT_DEFAULT_PM);
            (void)snprintf(s_msg, sizeof(s_msg), "4. hue %3lu, strength %u pm (F15)",
                           (unsigned long)h, (unsigned)LAMP_TINT_DEFAULT_PM);
            step(s_msg, 2000u);
        }
        for (uint32_t st = 250u; st <= 1000u; st += 250u)
        {
            static char s_msg[48];

            lamp_set_tint(160u, (uint16_t)st);   /* blue-ish: pick a favourite */
            (void)snprintf(s_msg, sizeof(s_msg), "   hue 160, strength %lu pm",
                           (unsigned long)st);
            step(s_msg, 2500u);
        }
        lamp_set_tint(0u, 0u);
        step("   tint off: plain warm white", 2000u);

        lamp_fade_to(0u, 10000u);
        step("5. fade down over 10 s ... interrupted after 4 s", 4000u);
        lamp_fade_to(1000u, 2000u);
        step("   -> back up from where it was: no jump", 3000u);

        lamp_fade_to(GLOW_PM, 10000u);
        step("6. fade to glow over 10 s (F8): 12 LEDs at W = 1", 12000u);
        lamp_pulse();
        step("   pulse at glow: soft rise (F9 when dark)", 3000u);
        lamp_fade_to(1u, 4000u);
        step("   glow 12 -> 6 -> 3 -> 1 LEDs (lowest = 1 LED at W = 1)", 6000u);

        lamp_fade_to(0u, 3000u);
        step("7. glow -> off in 3 s", 5000u);
    }
}
