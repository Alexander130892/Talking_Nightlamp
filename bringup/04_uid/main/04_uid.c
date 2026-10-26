/**
 * @file  04_uid.c
 * @brief Bring-up step 4: full presence cycle (ADR 0004).
 *        WUPA -> ANTICOLLISION -> SELECT -> HLTA, every 200 ms.
 *        Logs UID and SAK. Supports 4-byte and 7-byte UIDs (cascade).
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_timer.h"

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
#define REG_COLL         (0x0Eu)
#define REG_MODE         (0x11u)
#define REG_TX_MODE      (0x12u)
#define REG_RX_MODE      (0x13u)
#define REG_TX_CONTROL   (0x14u)
#define REG_TX_ASK       (0x15u)
#define REG_T_MODE       (0x2Au)
#define REG_T_PRESCALER  (0x2Bu)
#define REG_T_RELOAD_H   (0x2Cu)
#define REG_T_RELOAD_L   (0x2Du)
#define REG_VERSION      (0x37u)

/* Commands (CommandReg) */
#define CMD_IDLE         (0x00u)
#define CMD_TRANSMIT     (0x04u)
#define CMD_TRANSCEIVE   (0x0Cu)
#define CMD_SOFT_RESET   (0x0Fu)

/* ComIrqReg bits */
#define IRQ_TX           (0x40u)
#define IRQ_RX           (0x20u)
#define IRQ_IDLE         (0x10u)
#define IRQ_ERR          (0x02u)
#define IRQ_TIMER        (0x01u)

/* ErrorReg bits */
#define ERR_COLL         (0x08u)
#define ERR_FAIL_MASK    (0x17u) /* BufferOvfl | CRCErr | ParityErr | ProtocolErr */

/* ISO 14443-3 type A */
#define PICC_WUPA        (0x52u)
#define PICC_HLTA        (0x50u)
#define PICC_NVB_ANTICOLL (0x20u) /* 2 bytes sent, no UID bits known */
#define PICC_NVB_SELECT  (0x70u)  /* 7 bytes sent: full UID part + BCC */
#define PICC_CT          (0x88u)  /* cascade tag: UID continues */
#define SAK_CASCADE      (0x04u)  /* SAK bit: UID not complete yet */

#define UID_MAX_LEN      (10u)

typedef enum
{
    XFER_OK = 0,
    XFER_NO_ANSWER,
    XFER_COLLISION,
    XFER_BCC,
    XFER_ERROR
} xfer_t;

static const char *TAG = "uid";
static spi_device_handle_t s_dev;

/* ---- SPI register access ------------------------------------------- */

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

    reg_write(REG_T_MODE, 0x80u);        /* 25 ms answer timeout, TAuto */
    reg_write(REG_T_PRESCALER, 0xA9u);
    reg_write(REG_T_RELOAD_H, 0x03u);
    reg_write(REG_T_RELOAD_L, 0xE8u);

    reg_write(REG_TX_ASK, 0x40u);        /* 100 % ASK */
    reg_write(REG_MODE, 0x3Du);          /* CRC preset 0x6363 */
    reg_set_bits(REG_TX_CONTROL, 0x03u); /* antenna on */

    ESP_LOGI(TAG, "RC522 version 0x%02X, antenna on", reg_read(REG_VERSION));
}

/** CRC_A appended on TX and checked on RX by the RC522 itself. */
static void crc_enable(bool b_on)
{
    reg_write(REG_TX_MODE, b_on ? 0x80u : 0x00u);
    reg_write(REG_RX_MODE, b_on ? 0x80u : 0x00u);
}

/* ---- One RF exchange ----------------------------------------------- */

/**
 * Send p_tx over RF.
 * p_rx == NULL: transmit only, no answer expected (used for HLTA).
 * Otherwise:    transmit and collect the answer in p_rx.
 */
static xfer_t rf_exchange(const uint8_t *p_tx, uint8_t tx_len,
                          uint8_t tx_last_bits,
                          uint8_t *p_rx, uint8_t rx_max, uint8_t *p_rx_len)
{
    const bool    b_rx      = (NULL != p_rx);
    const uint8_t done_mask = b_rx ? (uint8_t)(IRQ_RX | IRQ_IDLE | IRQ_ERR)
                                   : IRQ_IDLE;
    xfer_t        result    = XFER_NO_ANSWER;
    int64_t       deadline  = esp_timer_get_time() + 40000;
    uint8_t       irq       = 0u;

    reg_write(REG_COMMAND, CMD_IDLE);
    reg_write(REG_COM_IRQ, 0x7Fu);
    reg_write(REG_FIFO_LEVEL, 0x80u);
    for (uint8_t i = 0u; i < tx_len; i++)
    {
        reg_write(REG_FIFO_DATA, p_tx[i]);
    }
    reg_write(REG_BIT_FRAMING, (uint8_t)(tx_last_bits & 0x07u));

    if (b_rx)
    {
        reg_write(REG_COMMAND, CMD_TRANSCEIVE);
        reg_set_bits(REG_BIT_FRAMING, 0x80u); /* StartSend */
    }
    else
    {
        reg_write(REG_COMMAND, CMD_TRANSMIT); /* starts immediately */
    }

    while (esp_timer_get_time() < deadline)
    {
        irq = reg_read(REG_COM_IRQ);
        if (0u != (irq & (uint8_t)(done_mask | IRQ_TIMER)))
        {
            break;
        }
    }
    reg_clear_bits(REG_BIT_FRAMING, 0x80u);

    if (!b_rx)
    {
        result = (0u != (irq & IRQ_IDLE)) ? XFER_OK : XFER_ERROR;
    }
    else if (0u != (irq & done_mask))
    {
        const uint8_t err = reg_read(REG_ERROR);

        if (0u != (err & ERR_COLL))
        {
            result = XFER_COLLISION;
        }
        else if (0u != (err & ERR_FAIL_MASK))
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
    else
    {
        /* timer expired: no answer */
    }

    return result;
}

/* ---- ISO 14443-3 commands ------------------------------------------ */

static xfer_t picc_wupa(uint8_t p_atqa[2])
{
    const uint8_t wupa = PICC_WUPA;
    uint8_t       len  = 0u;
    xfer_t        res;

    crc_enable(false);
    res = rf_exchange(&wupa, 1u, 7u, p_atqa, 2u, &len); /* short frame */
    if ((XFER_OK == res) && (2u != len))
    {
        res = XFER_ERROR;
    }
    return res;
}

/**
 * Anticollision + SELECT over all cascade levels.
 * Single card only: a collision is reported, not resolved.
 */
static xfer_t picc_select(uint8_t *p_uid, uint8_t *p_uid_len, uint8_t *p_sak,
                          uint8_t *p_sak_frame_len)
{
    static const uint8_t sel_cmd[3] = {0x93u, 0x95u, 0x97u};
    xfer_t  res = XFER_ERROR;

    *p_uid_len = 0u;

    for (uint8_t lvl = 0u; lvl < 3u; lvl++)
    {
        uint8_t ac_tx[2] = {sel_cmd[lvl], PICC_NVB_ANTICOLL};
        uint8_t uid_part[5];
        uint8_t sel_tx[7];
        uint8_t sak_rx[3];
        uint8_t n = 0u;

        /* ANTICOLLISION: whole bytes, no CRC; card answers UID part + BCC */
        crc_enable(false);
        reg_clear_bits(REG_COLL, 0x80u); /* ValuesAfterColl = 0 */
        res = rf_exchange(ac_tx, 2u, 0u, uid_part, 5u, &n);
        if (XFER_OK != res)
        {
            break;
        }
        if (5u != n)
        {
            res = XFER_ERROR;
            break;
        }
        if (0u != (uint8_t)(uid_part[0] ^ uid_part[1] ^ uid_part[2] ^
                            uid_part[3] ^ uid_part[4]))
        {
            res = XFER_BCC;
            break;
        }

        /* SELECT: 0x9x 0x70 + UID part + BCC, CRC_A by the RC522 */
        sel_tx[0] = sel_cmd[lvl];
        sel_tx[1] = PICC_NVB_SELECT;
        for (uint8_t i = 0u; i < 5u; i++)
        {
            sel_tx[2u + i] = uid_part[i];
        }
        crc_enable(true);
        res = rf_exchange(sel_tx, 7u, 0u, sak_rx, 3u, &n);
        crc_enable(false);
        if (XFER_OK != res)
        {
            break;
        }
        if (0u == n)
        {
            res = XFER_ERROR;
            break;
        }

        /* Collect UID bytes; a cascade tag 0x88 is not part of the UID */
        if (PICC_CT == uid_part[0])
        {
            for (uint8_t i = 1u; i < 4u; i++)
            {
                p_uid[(*p_uid_len)++] = uid_part[i];
            }
        }
        else
        {
            for (uint8_t i = 0u; i < 4u; i++)
            {
                p_uid[(*p_uid_len)++] = uid_part[i];
            }
        }

        if (0u == (sak_rx[0] & SAK_CASCADE))
        {
            *p_sak           = sak_rx[0];
            *p_sak_frame_len = n;
            res              = XFER_OK;
            break;
        }
        res = XFER_ERROR; /* cascade bit set: continue with next level */
    }

    return res;
}

static xfer_t picc_hlta(void)
{
    const uint8_t hlta[2] = {PICC_HLTA, 0x00u};
    xfer_t        res;

    crc_enable(true);
    res = rf_exchange(hlta, 2u, 0u, NULL, 0u, NULL); /* card stays silent */
    crc_enable(false);
    return res;
}

/* ---- Main loop ----------------------------------------------------- */

static const char *xfer_name(xfer_t r)
{
    static const char *names[] = {"ok", "no answer", "collision",
                                  "BCC error", "error"};
    return names[r];
}

void app_main(void)
{
    uint8_t  atqa[2];
    uint8_t  uid[UID_MAX_LEN];
    uint8_t  uid_len = 0u;
    uint8_t  sak     = 0u;
    uint8_t  sak_len = 0u;
    uint32_t n_poll  = 0u;
    char     uid_str[3u * UID_MAX_LEN + 1u];
    xfer_t   res;

    hw_init();
    rc522_setup();
    ESP_LOGI(TAG, "cycle WUPA -> ANTICOLL -> SELECT -> HLTA every %u ms",
             (unsigned)POLL_PERIOD_MS);

    for (;;)
    {
        n_poll++;
        res = picc_wupa(atqa);
        if (XFER_NO_ANSWER == res)
        {
            ESP_LOGI(TAG, "#%3lu  --", (unsigned long)n_poll);
        }
        else if (XFER_OK != res)
        {
            ESP_LOGW(TAG, "#%3lu  WUPA %s", (unsigned long)n_poll,
                     xfer_name(res));
        }
        else
        {
            res = picc_select(uid, &uid_len, &sak, &sak_len);
            if (XFER_OK == res)
            {
                for (uint8_t i = 0u; i < uid_len; i++)
                {
                    (void)snprintf(&uid_str[3u * i], 4u, "%02X ", uid[i]);
                }
                ESP_LOGI(TAG, "#%3lu  UID %s(%u B)  SAK %02X  [SAK frame %u B]"
                         "  HLTA %s",
                         (unsigned long)n_poll, uid_str, uid_len, sak, sak_len,
                         xfer_name(picc_hlta()));
            }
            else
            {
                ESP_LOGW(TAG, "#%3lu  ATQA %02X %02X, select %s",
                         (unsigned long)n_poll, atqa[0], atqa[1],
                         xfer_name(res));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(POLL_PERIOD_MS));
    }
}
