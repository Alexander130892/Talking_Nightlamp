/**
 * @file  02_rc522.c
 * @brief Bring-up step 2: MFRC522 over SPI3 (VSPI).
 *        Reads VersionReg and does a write/read-back on ModWidthReg.
 */

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"

#define PIN_SCK   (18)
#define PIN_MISO  (19)
#define PIN_MOSI  (23)
#define PIN_CS    (5)
#define PIN_RST   (22)

#define REG_MOD_WIDTH  (0x24u)  /* reset value 0x26, safe to write */
#define REG_VERSION    (0x37u)

static const char *TAG = "rc522";
static spi_device_handle_t s_dev;

static uint8_t reg_read(uint8_t reg)
{
    spi_transaction_t t =
    {
        .flags   = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
        .length  = 16u,                                   /* bits */
        .tx_data = { (uint8_t)(0x80u | (reg << 1)), 0x00u },
    };

    ESP_ERROR_CHECK(spi_device_transmit(s_dev, &t));
    return t.rx_data[1];                                  /* 2nd byte */
}

static void reg_write(uint8_t reg, uint8_t val)
{
    spi_transaction_t t =
    {
        .flags   = SPI_TRANS_USE_TXDATA,
        .length  = 16u,
        .tx_data = { (uint8_t)((reg << 1) & 0x7Eu), val },
    };

    ESP_ERROR_CHECK(spi_device_transmit(s_dev, &t));
}

static void rc522_hard_reset(void)
{
    const gpio_config_t cfg =
    {
        .pin_bit_mask = (1ULL << PIN_RST),
        .mode         = GPIO_MODE_OUTPUT,
    };

    ESP_ERROR_CHECK(gpio_config(&cfg));
    ESP_ERROR_CHECK(gpio_set_level(PIN_RST, 0));
    vTaskDelay(pdMS_TO_TICKS(10u));
    ESP_ERROR_CHECK(gpio_set_level(PIN_RST, 1));
    vTaskDelay(pdMS_TO_TICKS(50u));                       /* oscillator start */
}

static void spi_init(void)
{
    const spi_bus_config_t bus =
    {
        .sclk_io_num   = PIN_SCK,
        .miso_io_num   = PIN_MISO,
        .mosi_io_num   = PIN_MOSI,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    const spi_device_interface_config_t dev =
    {
        .clock_speed_hz = 1000000,                        /* 1 MHz */
        .mode           = 0,                              /* CPOL 0, CPHA 0 */
        .spics_io_num   = PIN_CS,
        .queue_size     = 1,
    };

    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &bus, SPI_DMA_DISABLED));
    ESP_ERROR_CHECK(spi_bus_add_device(SPI3_HOST, &dev, &s_dev));
}

void app_main(void)
{
    uint8_t version;
    uint8_t readback;

    rc522_hard_reset();
    spi_init();

    version = reg_read(REG_VERSION);
    ESP_LOGI(TAG, "VersionReg = 0x%02X", version);
    if ((0x00u == version) || (0xFFu == version))
    {
        ESP_LOGE(TAG, "no answer: check power, wiring, MISO");
    }

    reg_write(REG_MOD_WIDTH, 0x55u);
    readback = reg_read(REG_MOD_WIDTH);
    ESP_LOGI(TAG, "ModWidthReg write 0x55 -> read 0x%02X %s", readback,
             (0x55u == readback) ? "(MOSI ok)" : "(MISMATCH)");
    reg_write(REG_MOD_WIDTH, 0x26u);                      /* restore */

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000u));
    }
}