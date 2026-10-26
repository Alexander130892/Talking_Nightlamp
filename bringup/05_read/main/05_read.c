/**
 * @file  05_read.c
 * @brief Bring-up step 5: read the story record from a MIFARE Classic card.
 *        WUPA -> SELECT -> AUTH (key A, sector 1) -> READ block 4
 *        -> HLTA -> Crypto1 off, every 200 ms.
 *        The 16 bytes are parsed with the firmware's tag_parse().
 *
 * Experiment: set CRYPTO1_OFF to 0 to see the reader go deaf.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "tag_format.h"

#define CRYPTO1_OFF      (1)     /* 0 = forget to clear MFCrypto1On */
#define POLL_PERIOD_MS   (200u)
#define RECORD_BLOCK     (4u)    /* sector 1, block 0 (card-format.md) */

#define PIN_SCK   (18)
#define PIN_MISO  (19)
#define PIN_MOSI  (23)
#define PIN_CS    (5)
#define PIN_RST   (22)

/* MFRC522 registers (datasheet section 9) */
#define REG_COMMAND      (0x01u)
#define REG_COM_IRQ      (0x04u)
#define REG_ERROR        (0x06u)
#define REG_STATUS2      (0x08u)
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
#define CMD_MF_AUTHENT   (0x0Eu)
#define CMD_SOFT_RESET   (0x0Fu)

/* ComIrqReg bits */
#define IRQ_RX           (0x20u)
#define IRQ_IDLE         (0x10u)
#define IRQ_ERR          (0x02u)
#define IRQ_TIMER        (0x01u)

/* ErrorReg bits */
#define ERR_COLL         (0x08u)
#define ERR_FAIL_MASK    (0x17u) /* BufferOvfl | CRCErr | ParityErr | ProtocolErr */

/* Status2Reg bits */
#define STATUS2_CRYPTO1_ON (0x08u)

/* ISO 14443-3 type A / MIFARE Classic */
#define PICC_WUPA        (0x52u)
#define PICC_HLTA        (0x50u)
#define PICC_NVB_ANTICOLL (0x20u)
#define PICC_NVB_SELECT  (0x70u)
#define PICC_CT          (0x88u)
#define SAK_CASCADE      (0x04u)
#define MF_AUTH_KEY_A    (0x60u)
#define MF_READ          (0x30u)

#define UID_MAX_LEN      (10u)
#define MF_BLOCK_LEN     (16u)
#define MF_KEY_LEN       (6u)

typedef enum
{
    XFER_OK = 0,
    XFER_NO_ANSWER,
    XFER_COLLISION,
    XFER_BCC,
    XFER_AUTH,
    XFER_ERROR
} xfer_t;

static const char *TAG = "read";
static spi_device_handle_t s_dev;
static uint8_t s_last_err; /* ErrorReg after the last exchange, for logs */

static const uint8_t KEY_DEFAULT[MF_KEY_LEN] =
{
    0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu
};

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

static void crc_enable(bool b_on)
{
    reg_write(REG_TX_MODE, b_on ? 0x80u : 0x00u);
    reg_write(REG_RX_MODE, b_on ? 0x80u : 0x00u);
}

/* ---- RC522 command execution --------------------------------------- */

/** Load the FIFO and start cmd. StartSend only applies to Transceive. */
static void cmd_start(uint8_t cmd, const uint8_t *p_tx, uint8_t tx_len,
                      uint8_t tx_last_bits)
{
    reg_write(REG_COMMAND, CMD_IDLE);
    reg_write(REG_COM_IRQ, 0x7Fu);
    reg_write(REG_FIFO_LEVEL, 0x80u);
    for (uint8_t i = 0u; i < tx_len; i++)
    {
        reg_write(REG_FIFO_DATA, p_tx[i]);
    }
    reg_write(REG_BIT_FRAMING, (uint8_t)(tx_last_bits & 0x07u));
    reg_write(REG_COMMAND, cmd);
    if (CMD_TRANSCEIVE == cmd)
    {
        reg_set_bits(REG_BIT_FRAMING, 0x80u); /* StartSend */
    }
}

/** Wait until any bit in mask (or the answer timer) fires. */
static uint8_t cmd_wait(uint8_t mask)
{
    const int64_t deadline = esp_timer_get_time() + 40000;
    uint8_t       irq      = 0u;

    while (esp_timer_get_time() < deadline)
    {
        irq = reg_read(REG_COM_IRQ);
        if (0u != (irq & (uint8_t)(mask | IRQ_TIMER)))
        {
            break;
        }
    }
    reg_clear_bits(REG_BIT_FRAMING, 0x80u);
    s_last_err = reg_read(REG_ERROR);
    return irq;
}

/** Transceive: send p_tx, collect the answer. */
static xfer_t rf_transceive(const uint8_t *p_tx, uint8_t tx_len,
                            uint8_t tx_last_bits,
                            uint8_t *p_rx, uint8_t rx_max, uint8_t *p_rx_len)
{
    const uint8_t done = (uint8_t)(IRQ_RX | IRQ_IDLE | IRQ_ERR);
    xfer_t        result;
    uint8_t       irq;

    cmd_start(CMD_TRANSCEIVE, p_tx, tx_len, tx_last_bits);
    irq = cmd_wait(done);

    if (0u == (irq & done))
    {
        result = XFER_NO_ANSWER;
    }
    else if (0u != (s_last_err & ERR_COLL))
    {
        result = XFER_COLLISION;
    }
    else if (0u != (s_last_err & ERR_FAIL_MASK))
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

    return result;
}

/** Transmit only, no answer expected (HLTA). */
static xfer_t rf_transmit(const uint8_t *p_tx, uint8_t tx_len)
{
    cmd_start(CMD_TRANSMIT, p_tx, tx_len, 0u);
    return (0u != (cmd_wait(IRQ_IDLE) & IRQ_IDLE)) ? XFER_OK : XFER_ERROR;
}

/* ---- ISO 14443-3 / MIFARE Classic commands ------------------------- */

static xfer_t picc_wupa(uint8_t p_atqa[2])
{
    const uint8_t wupa = PICC_WUPA;
    uint8_t       len  = 0u;
    xfer_t        res;

    crc_enable(false);
    res = rf_transceive(&wupa, 1u, 7u, p_atqa, 2u, &len);
    if ((XFER_OK == res) && (2u != len))
    {
        res = XFER_ERROR;
    }
    return res;
}

static xfer_t picc_select(uint8_t *p_uid, uint8_t *p_uid_len, uint8_t *p_sak)
{
    static const uint8_t sel_cmd[3] = {0x93u, 0x95u, 0x97u};
    xfer_t res = XFER_ERROR;

    *p_uid_len = 0u;

    for (uint8_t lvl = 0u; lvl < 3u; lvl++)
    {
        uint8_t ac_tx[2] = {sel_cmd[lvl], PICC_NVB_ANTICOLL};
        uint8_t part[5];
        uint8_t sel_tx[7];
        uint8_t sak_rx[3];
        uint8_t n = 0u;

        crc_enable(false);
        reg_clear_bits(REG_COLL, 0x80u);
        res = rf_transceive(ac_tx, 2u, 0u, part, 5u, &n);
        if (XFER_OK != res)
        {
            break;
        }
        if (5u != n)
        {
            res = XFER_ERROR;
            break;
        }
        if (0u != (uint8_t)(part[0] ^ part[1] ^ part[2] ^ part[3] ^ part[4]))
        {
            res = XFER_BCC;
            break;
        }

        sel_tx[0] = sel_cmd[lvl];
        sel_tx[1] = PICC_NVB_SELECT;
        (void)memcpy(&sel_tx[2], part, 5u);
        crc_enable(true);
        res = rf_transceive(sel_tx, 7u, 0u, sak_rx, 3u, &n);
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

        if (PICC_CT == part[0])
        {
            (void)memcpy(&p_uid[*p_uid_len], &part[1], 3u);
            *p_uid_len = (uint8_t)(*p_uid_len + 3u);
        }
        else
        {
            (void)memcpy(&p_uid[*p_uid_len], &part[0], 4u);
            *p_uid_len = (uint8_t)(*p_uid_len + 4u);
        }

        if (0u == (sak_rx[0] & SAK_CASCADE))
        {
            *p_sak = sak_rx[0];
            res    = XFER_OK;
            break;
        }
        res = XFER_ERROR;
    }

    return res;
}

/**
 * MIFARE Classic 3-pass authentication, done by the RC522 (MFAuthent).
 * FIFO: 60 | block | key[6] | last 4 UID bytes.
 * Success = MFCrypto1On set in Status2Reg.
 */
static xfer_t mf_auth_key_a(uint8_t block, const uint8_t key[MF_KEY_LEN],
                            const uint8_t *p_uid, uint8_t uid_len)
{
    uint8_t buf[2u + MF_KEY_LEN + 4u];

    buf[0] = MF_AUTH_KEY_A;
    buf[1] = block;
    (void)memcpy(&buf[2], key, MF_KEY_LEN);
    (void)memcpy(&buf[2u + MF_KEY_LEN], &p_uid[uid_len - 4u], 4u);

    cmd_start(CMD_MF_AUTHENT, buf, (uint8_t)sizeof(buf), 0u);
    (void)cmd_wait((uint8_t)(IRQ_IDLE | IRQ_ERR));

    return (0u != (reg_read(REG_STATUS2) & STATUS2_CRYPTO1_ON)) ? XFER_OK
                                                                : XFER_AUTH;
}

static xfer_t mf_read_block(uint8_t block, uint8_t p_out[MF_BLOCK_LEN])
{
    const uint8_t cmd[2] = {MF_READ, block};
    uint8_t       rx[MF_BLOCK_LEN + 2u];
    uint8_t       n = 0u;
    xfer_t        res;

    crc_enable(true);
    res = rf_transceive(cmd, 2u, 0u, rx, (uint8_t)sizeof(rx), &n);
    crc_enable(false);
    if ((XFER_OK == res) && (n < MF_BLOCK_LEN))
    {
        res = XFER_ERROR;   /* e.g. a 4-bit NAK: access denied */
    }
    if (XFER_OK == res)
    {
        (void)memcpy(p_out, rx, MF_BLOCK_LEN);
    }
    return res;
}

static void picc_hlta(void)
{
    const uint8_t hlta[2] = {PICC_HLTA, 0x00u};

    crc_enable(true);
    (void)rf_transmit(hlta, 2u);
    crc_enable(false);
}

/* ---- One poll ------------------------------------------------------ */

typedef struct
{
    xfer_t  res;         /* first failing step, or XFER_OK */
    char    step[8];     /* name of that step */
    uint8_t uid[UID_MAX_LEN];
    uint8_t uid_len;
    uint8_t sak;
    uint8_t block[MF_BLOCK_LEN];
    uint8_t err;         /* ErrorReg of the failing step */
} poll_t;

static void poll_once(poll_t *p_out)
{
    uint8_t atqa[2];

    (void)memset(p_out, 0, sizeof(*p_out));

    p_out->res = picc_wupa(atqa);
    if (XFER_NO_ANSWER == p_out->res)
    {
        return;  /* nothing answered: nothing to clean up */
    }

    (void)strcpy(p_out->step, "wupa");
    if (XFER_OK == p_out->res)
    {
        (void)strcpy(p_out->step, "select");
        p_out->res = picc_select(p_out->uid, &p_out->uid_len, &p_out->sak);
    }
    if (XFER_OK == p_out->res)
    {
        (void)strcpy(p_out->step, "auth");
        p_out->res = mf_auth_key_a(RECORD_BLOCK, KEY_DEFAULT, p_out->uid,
                                   p_out->uid_len);
    }
    if (XFER_OK == p_out->res)
    {
        (void)strcpy(p_out->step, "read");
        p_out->res = mf_read_block(RECORD_BLOCK, p_out->block);
    }
    p_out->err = s_last_err; /* snapshot before cleanup overwrites it */
    /* Cleanup on EVERY path once a card answered (ADR 0004). */
    picc_hlta();
#if (CRYPTO1_OFF != 0)
    reg_clear_bits(REG_STATUS2, STATUS2_CRYPTO1_ON);
    if (0u != (reg_read(REG_STATUS2) & STATUS2_CRYPTO1_ON))
    {
        ESP_LOGE(TAG, "Crypto1 still ON after clear: write lost (wiring?)");
    }
#endif
}

/* ---- Main loop ----------------------------------------------------- */

static const char *xfer_name(xfer_t r)
{
    static const char *names[] = {"ok", "no answer", "collision",
                                  "BCC error", "auth failed", "error"};
    return names[r];
}

static const char *tag_name(tag_status_t s)
{
    static const char *names[] = {"OK", "ERR_ARG", "ERR_LEN", "ERR_MAGIC",
                                  "ERR_VERSION", "ERR_STORY"};
    return names[s];
}

static void hex(char *p_dst, const uint8_t *p_src, uint8_t len)
{
    for (uint8_t i = 0u; i < len; i++)
    {
        (void)snprintf(&p_dst[3u * i], 4u, "%02X ", p_src[i]);
    }
}

void app_main(void)
{
    poll_t   p;
    uint32_t n_poll = 0u;
    char     uid_str[3u * UID_MAX_LEN + 1u];
    char     blk_str[3u * MF_BLOCK_LEN + 1u];

    hw_init();
    rc522_setup();
    ESP_LOGI(TAG, "reading block %u with default key A every %u ms, "
             "Crypto1 off after poll: %d",
             (unsigned)RECORD_BLOCK, (unsigned)POLL_PERIOD_MS, CRYPTO1_OFF);

    for (;;)
    {
        n_poll++;
        poll_once(&p);

        if (XFER_NO_ANSWER == p.res)
        {
            ESP_LOGI(TAG, "#%3lu  --", (unsigned long)n_poll);
        }
        else if (XFER_OK != p.res)
        {
            ESP_LOGW(TAG, "#%3lu  %s: %s (ErrorReg %02X)",
                     (unsigned long)n_poll, p.step, xfer_name(p.res),
                     p.err);
        }
        else
        {
            tag_record_t rec = {0};
            tag_status_t ts  = tag_parse(p.block, MF_BLOCK_LEN, &rec);

            uid_str[0] = '\0';
            hex(uid_str, p.uid, p.uid_len);
            hex(blk_str, p.block, MF_BLOCK_LEN);
            ESP_LOGI(TAG, "#%3lu  UID %s SAK %02X", (unsigned long)n_poll,
                     uid_str, p.sak);
            ESP_LOGI(TAG, "       block %u: %s", (unsigned)RECORD_BLOCK,
                     blk_str);
            if (TAG_OK == ts)
            {
                ESP_LOGI(TAG, "       record: story %u, hue %u",
                         rec.story_id, rec.hue);
            }
            else
            {
                ESP_LOGI(TAG, "       record: %s", tag_name(ts));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(POLL_PERIOD_MS));
    }
}
