/**
 * @file  03_detect.c
 * @brief Bring-up step 3: card presence with WUPA (ISO 14443-3 short frame).
 *        Sends WUPA every 200 ms through the MFRC522 and logs the ATQA.
 *
 * Experiment: run first with WUPA_RETRY = 0 and a card left on the reader.
 */

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_timer.h"

#define WUPA_RETRY       (1)     /* 0 = single WUPA per poll, 1 = retry once */
#define POLL_PERIOD_MS   (200u)

#define PIN_SCK   (18)
#define PIN_MISO  (19)
#define PIN_MOSI  (23)

#define PIN_CS    (5)
#define PIN_RST   (22)

/* MFRC522 registers (datasheet section 9) */
#define REG_COMMAND      (0x01u)
#define REG_COM_IRQ      (0x04u)
#define REG_ERROR        (0x06u)
#define REG_FIFO_DATA    (0x09u)
#define REG_FIFO_LEVEL   (0x0Au)
#define REG_BIT_FRAMING  (0x0Du)
#define REG_MODE         (0x11u)
#define REG_TX_CONTROL   (0x14u)
#define REG_TX_ASK       (0x15u)
#define REG_T_MODE       (0x2Au)
#define REG_T_PRESCALER  (0x2Bu)
#define REG_T_RELOAD_H   (0x2Cu)
#define REG_T_RELOAD_L   (0x2Du)
#define REG_VERSION      (0x37u)

/* Commands (CommandReg) */
#define CMD_IDLE         (0x00u)
#define CMD_TRANSCEIVE   (0x0Cu)
#define CMD_SOFT_RESET   (0x0Fu)

/* ComIrqReg bits */
#define IRQ_RX           (0x20u)
#define IRQ_IDLE         (0x10u)
#define IRQ_TIMER        (0x01u)

/* ErrorReg: BufferOvfl | ParityErr | ProtocolErr */
#define ERR_MASK         (0x13u)

/* ISO 14443-3 type A short-frame commands */
#define PICC_WUPA        (0x52u)


typedef enum
{
    XFER_OK = 0,
    XFER_NO_ANSWER,
    XFER_ERROR
} xfer_t;

static const char *TAG = "detect";
static spi_device_handle_t s_dev;

/* ---- SPI register access (as in 02_rc522) -------------------------- */

static uint8_t reg_read(uint8_t reg)
{
    spi_transaction_t t =
    {
        .flags   = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
        .length  = 16u,
        .tx_data = { (uint8_t)(0x80u | (reg << 1)), 0x00u },
    };

    ESP_ERROR_CHECK(spi_device_transmit(s_dev, &t));
    return t.rx_data[1];
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

static void reg_set_bits(uint8_t reg, uint8_t mask)
{
    reg_write(reg, (uint8_t)(reg_read(reg) | mask));
}

static void reg_clear_bits(uint8_t reg, uint8_t mask)
{
    reg_write(reg, (uint8_t)(reg_read(reg) & (uint8_t)~mask));
}

/* ---- Chip setup ---------------------------------------------------- */

static void hw_init(void)
{
    const gpio_config_t rst =
    {
        .pin_bit_mask = (1ULL << PIN_RST),
        .mode         = GPIO_MODE_OUTPUT,
    };
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
        .clock_speed_hz = 1000000,
        .mode           = 0,
        .spics_io_num   = PIN_CS,
        .queue_size     = 1,
    };

    ESP_ERROR_CHECK(gpio_config(&rst));
    ESP_ERROR_CHECK(gpio_set_level(PIN_RST, 0));
    vTaskDelay(pdMS_TO_TICKS(10u));
    ESP_ERROR_CHECK(gpio_set_level(PIN_RST, 1));
    vTaskDelay(pdMS_TO_TICKS(50u));

    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &bus, SPI_DMA_DISABLED));
    ESP_ERROR_CHECK(spi_bus_add_device(SPI3_HOST, &dev, &s_dev));
}

static void rc522_setup(void)
{
    reg_write(REG_COMMAND, CMD_SOFT_RESET);
    vTaskDelay(pdMS_TO_TICKS(50u));

    /* Timer: 13.56 MHz / (2 * 0xA9 + 1) = 40 kHz -> 25 us/tick,
     * reload 1000 -> 25 ms answer timeout. TAuto: starts after TX. */
    reg_write(REG_T_MODE, 0x80u);
    reg_write(REG_T_PRESCALER, 0xA9u);
    reg_write(REG_T_RELOAD_H, 0x03u);
    reg_write(REG_T_RELOAD_L, 0xE8u);

    reg_write(REG_TX_ASK, 0x40u);   /* 100 % ASK, required by type A */
    reg_write(REG_MODE, 0x3Du);     /* CRC preset 0x6363 (ISO 14443-3) */

    reg_set_bits(REG_TX_CONTROL, 0x03u); /* antenna on: TX1 + TX2 */

    ESP_LOGI(TAG, "RC522 version 0x%02X, antenna on", reg_read(REG_VERSION));
}

/* ---- One RF exchange ----------------------------------------------- */

/**
 * Send tx over RF and collect the answer.
 * @param tx_last_bits  bits of the last byte to send (0 = whole byte)
 */
static xfer_t transceive(const uint8_t *p_tx, uint8_t tx_len,
                         uint8_t tx_last_bits,
                         uint8_t *p_rx, uint8_t rx_max, uint8_t *p_rx_len)
{
    xfer_t  result   = XFER_NO_ANSWER;
    int64_t deadline = esp_timer_get_time() + 40000;   /* > 25 ms timer */
    uint8_t irq      = 0u;

    reg_write(REG_COMMAND, CMD_IDLE);
    reg_write(REG_COM_IRQ, 0x7Fu);          /* clear all IRQ flags */
    reg_write(REG_FIFO_LEVEL, 0x80u);       /* flush FIFO */
    for (uint8_t i = 0u; i < tx_len; i++)
    {
        reg_write(REG_FIFO_DATA, p_tx[i]);
    }
    reg_write(REG_BIT_FRAMING, (uint8_t)(tx_last_bits & 0x07u));
    reg_write(REG_COMMAND, CMD_TRANSCEIVE);
    reg_set_bits(REG_BIT_FRAMING, 0x80u);   /* StartSend */

    while (esp_timer_get_time() < deadline)
    {
        irq = reg_read(REG_COM_IRQ);
        if (0u != (irq & (IRQ_RX | IRQ_IDLE | IRQ_TIMER)))
        {
            break;
        }
    }
    reg_clear_bits(REG_BIT_FRAMING, 0x80u);

    if (0u != (irq & (IRQ_RX | IRQ_IDLE)))
    {
        if (0u != (reg_read(REG_ERROR) & ERR_MASK))
        {
            result = XFER_ERROR;
        }
        else
        {
            uint8_t n = (uint8_t)(reg_read(REG_FIFO_LEVEL) & 0x7Fu);

            n = (n > rx_max) ? rx_max : n;
            for (uint8_t i = 0u; i < n; i++)
            {
                p_rx[i] = reg_read(REG_FIFO_DATA);
            }
            *p_rx_len = n;
            result    = XFER_OK;
        }
    }

    return result;
}

static xfer_t send_wupa(uint8_t p_atqa[2], uint8_t *p_len)
{
    const uint8_t wupa = PICC_WUPA;

    /* Short frame: transmit only 7 bits of the byte. */
    return transceive(&wupa, 1u, 7u, p_atqa, 2u, p_len);
}

void app_main(void)
{
    uint8_t  atqa[2] = {0u, 0u};
    uint8_t  len     = 0u;
    uint32_t n_poll  = 0u;
    xfer_t   res;

    hw_init();
    rc522_setup();
    ESP_LOGI(TAG, "polling WUPA every %u ms, retry %d",
             (unsigned)POLL_PERIOD_MS, WUPA_RETRY);

    for (;;)
    {
        n_poll++;
        res = send_wupa(atqa, &len);
#if (WUPA_RETRY != 0)
        if (XFER_NO_ANSWER == res)
        {
            res = send_wupa(atqa, &len);
        }
#endif
        if ((XFER_OK == res) && (2u == len))
        {
            ESP_LOGI(TAG, "#%3lu  card   ATQA %02X %02X",
                     (unsigned long)n_poll, atqa[0], atqa[1]);
        }
        else if (XFER_ERROR == res)
        {
            ESP_LOGW(TAG, "#%3lu  error  (collision or noise)",
                     (unsigned long)n_poll);
        }
        else
        {
            ESP_LOGI(TAG, "#%3lu  --", (unsigned long)n_poll);
        }

        vTaskDelay(pdMS_TO_TICKS(POLL_PERIOD_MS));
    }
}
