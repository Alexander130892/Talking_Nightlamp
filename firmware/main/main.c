/**
 * @file  main.c
 * @brief Entry point. Hands over to app.
 */

#include "esp_log.h"
#include "app.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "Talking Nightlamp booting");

    app_start();
}
