/**
 * @file  01_blink.c
 * @brief Bring-up step 1: toolchain, flash and serial check.
 *        Toggles the on-board LED (GPIO 2) every 500 ms and logs its state.
 */

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define BLINK_GPIO       (GPIO_NUM_2)
#define BLINK_PERIOD_MS  (500u)

static const char *TAG = "blink";

void app_main(void)
{
    uint32_t level = 0u;
    const gpio_config_t cfg =
    {
        .pin_bit_mask = (1ULL << BLINK_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&cfg));
    ESP_LOGI(TAG, "blinking GPIO %d every %u ms", BLINK_GPIO,
             (unsigned)BLINK_PERIOD_MS);

    for (;;)
    {
        level ^= 1u;
        ESP_ERROR_CHECK(gpio_set_level(BLINK_GPIO, level));
        ESP_LOGI(TAG, "LED %s", (0u != level) ? "on" : "off");
        vTaskDelay(pdMS_TO_TICKS(BLINK_PERIOD_MS));
    }
}