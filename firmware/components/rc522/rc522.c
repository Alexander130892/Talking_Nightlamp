/**
 * @file  rc522.c
 * @brief MFRC522 driver (see rc522.h). Register-level code from bring-up 06.
 */

#include "rc522.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"

#define RECORD_BLOCK     (4u)    /* Classic: sector 1, block 0 */
#define NTAG_FIRST_PAGE  (4u)    /* NTAG: pages 4..7 = 16 bytes */
#define NTAG_PAGES       (4u)
#define NTAG_WRITE       (0xA2u)

_Static_assert((RECORD_BLOCK != 0u) && ((RECORD_BLOCK % 4u) != 3u),
               "record block must not be block 0 or a sector trailer");

/* MFRC522 registers (datasheet section 9) */
#define REG_COMMAND      (0x01u)
#define REG_COM_IRQ      (0x04u)
#define REG_ERROR        (0x06u)
#define REG_STATUS2      (0x08u)
#define REG_FIFO_DATA    (0x09u)
#define REG_FIFO_LEVEL   (0x0Au)
#define REG_CONTROL      (0x0Cu)
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
#define MF_WRITE         (0xA0u)
#define MF_ACK           (0x0Au)  /* 4-bit acknowledge */

#define MF_BLOCK_LEN     (16u)
#define MF_KEY_LEN       (6u)

typedef enum
{
    XFER_OK = 0,
    XFER_NO_ANSWER,
    XFER_COLLISION,
    XFER_BCC,
    XFER_AUTH,
    XFER_NAK,
    XFER_REFUSED,
    XFER_VERIFY,
    XFER_ERROR
} xfer_t;

static const char *TAG = "rc522";
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

    (void)spi_device_transmit(s_dev, &t);
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

    (void)spi_device_transmit(s_dev, &t);
}

static void reg_set_bits(uint8_t reg, uint8_t mask)
{
    reg_write(reg, (uint8_t)(reg_read(reg) | mask));
}

static void reg_clear_bits(uint8_t reg, uint8_t mask)
{
    reg_write(reg, (uint8_t)(reg_read(reg) & (uint8_t)~mask));
}

/** CRC_A generation on TX and checking on RX, set independently. */
static void crc_set(bool b_tx, bool b_rx)
{
    reg_write(REG_TX_MODE, b_tx ? 0x80u : 0x00u);
    reg_write(REG_RX_MODE, b_rx ? 0x80u : 0x00u);
}

/* ---- RC522 command execution --------------------------------------- */

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

/**
 * Transceive: send p_tx, collect the answer.
 * *p_rx_bits: valid bits in the last received byte (0 = whole byte).
 */
static xfer_t rf_transceive(const uint8_t *p_tx, uint8_t tx_len,
                            uint8_t tx_last_bits,
                            uint8_t *p_rx, uint8_t rx_max, uint8_t *p_rx_len,
                            uint8_t *p_rx_bits)
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
        if (NULL != p_rx_bits)
        {
            *p_rx_bits = (uint8_t)(reg_read(REG_CONTROL) & 0x07u);
        }
        result = XFER_OK;
    }

    return result;
}

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

    crc_set(false, false);
    res = rf_transceive(&wupa, 1u, 7u, p_atqa, 2u, &len, NULL);
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

        crc_set(false, false);
        reg_clear_bits(REG_COLL, 0x80u);
        res = rf_transceive(ac_tx, 2u, 0u, part, 5u, &n, NULL);
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
        crc_set(true, true);
        res = rf_transceive(sel_tx, 7u, 0u, sak_rx, 3u, &n, NULL);
        crc_set(false, false);
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

    crc_set(true, true);
    res = rf_transceive(cmd, 2u, 0u, rx, (uint8_t)sizeof(rx), &n, NULL);
    crc_set(false, false);
    if ((XFER_OK == res) && (n < MF_BLOCK_LEN))
    {
        res = XFER_ERROR;
    }
    if (XFER_OK == res)
    {
        (void)memcpy(p_out, rx, MF_BLOCK_LEN);
    }
    return res;
}

/** Whitelist: the only block this firmware may ever write. */
static bool block_writable(uint8_t block)
{
    return (RECORD_BLOCK == block);
}

/** One write phase: send p_tx with CRC, expect the 4-bit ACK (no CRC). */
static xfer_t mf_send_expect_ack(const uint8_t *p_tx, uint8_t tx_len)
{
    uint8_t rx   = 0u;
    uint8_t n    = 0u;
    uint8_t bits = 0u;
    xfer_t  res;

    crc_set(true, false);   /* CRC on TX, none on the 4-bit ACK */
    res = rf_transceive(p_tx, tx_len, 0u, &rx, 1u, &n, &bits);
    crc_set(false, false);

    if (XFER_OK == res)
    {
        if ((1u != n) || (4u != bits))
        {
            res = XFER_ERROR;
        }
        else if (MF_ACK != (rx & 0x0Fu))
        {
            res = XFER_NAK;
        }
        else
        {
            /* ACK */
        }
    }
    return res;
}

static xfer_t mf_write_block(uint8_t block, const uint8_t p_data[MF_BLOCK_LEN])
{
    const uint8_t cmd[2] = {MF_WRITE, block};
    xfer_t        res;

    if (!block_writable(block))
    {
        return XFER_REFUSED;
    }

    res = mf_send_expect_ack(cmd, 2u);                    /* phase 1 */
    if (XFER_OK == res)
    {
        res = mf_send_expect_ack(p_data, MF_BLOCK_LEN);   /* phase 2 */
    }
    return res;
}

static void picc_hlta(void)
{
    const uint8_t hlta[2] = {PICC_HLTA, 0x00u};

    crc_set(true, false);
    (void)rf_transmit(hlta, 2u);
    crc_set(false, false);
}


/* ---- Sessions ------------------------------------------------------ */

static esp_err_t map_xfer(xfer_t r)
{
    switch (r)
    {
        case XFER_OK:        return ESP_OK;
        case XFER_COLLISION: return ESP_ERR_INVALID_RESPONSE;
        case XFER_AUTH:      return ESP_ERR_INVALID_STATE;
        default:             return ESP_ERR_INVALID_CRC;
    }
}

/** WUPA + select. ESP_ERR_NOT_FOUND only if nothing answered the WUPA. */
static esp_err_t wake_select(rc522_tag_t *p_tag)
{
    uint8_t atqa[2];
    xfer_t  res = picc_wupa(atqa);

    if (XFER_NO_ANSWER == res)
    {
        return ESP_ERR_NOT_FOUND;
    }
    if (XFER_OK == res)
    {
        res = picc_select(p_tag->uid, &p_tag->uid_len, &p_tag->sak);
    }
    return map_xfer(res);
}

/** HLTA and Crypto1 off: on every path once a tag answered (ADR 0004). */
static void end_session(void)
{
    picc_hlta();
    reg_clear_bits(REG_STATUS2, STATUS2_CRYPTO1_ON);
    if (0u != (reg_read(REG_STATUS2) & STATUS2_CRYPTO1_ON))
    {
        ESP_LOGE(TAG, "Crypto1 still on after clear (wiring?)");
    }
}

static bool is_classic(uint8_t sak)
{
    return 0u != (sak & 0x08u);    /* Mini 0x09, 1K 0x08, 4K 0x18 */
}

/** Wake p_tag again; ESP_ERR_NOT_FOUND if it is not the one answering. */
static esp_err_t reselect(const rc522_tag_t *p_tag)
{
    rc522_tag_t now = { 0 };
    esp_err_t   err = wake_select(&now);

    if ((ESP_OK == err) &&
        ((now.uid_len != p_tag->uid_len) ||
         (0 != memcmp(now.uid, p_tag->uid, now.uid_len))))
    {
        err = ESP_ERR_NOT_FOUND;
    }
    return err;
}

/** Selected tag: authenticate if needed and read the record. */
static esp_err_t read_selected(const rc522_tag_t *p_tag, uint8_t p_buf[RC522_RECORD_LEN])
{
    xfer_t res;

    if (is_classic(p_tag->sak))
    {
        res = mf_auth_key_a(RECORD_BLOCK, KEY_DEFAULT, p_tag->uid, p_tag->uid_len);
        if (XFER_OK == res)
        {
            res = mf_read_block(RECORD_BLOCK, p_buf);
        }
    }
    else if (RC522_SAK_ULTRALIGHT == p_tag->sak)
    {
        res = mf_read_block(NTAG_FIRST_PAGE, p_buf);   /* READ returns 4 pages */
    }
    else
    {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return map_xfer(res);
}

static esp_err_t write_selected(const rc522_tag_t *p_tag, const uint8_t p_buf[RC522_RECORD_LEN])
{
    xfer_t res = XFER_OK;

    if (is_classic(p_tag->sak))
    {
        res = mf_write_block(RECORD_BLOCK, p_buf);     /* whitelisted */
    }
    else
    {
        for (uint8_t i = 0u; (i < NTAG_PAGES) && (XFER_OK == res); i++)
        {
            uint8_t cmd[6] = { NTAG_WRITE, (uint8_t)(NTAG_FIRST_PAGE + i) };

            (void)memcpy(&cmd[2], &p_buf[4u * i], 4u);
            res = mf_send_expect_ack(cmd, (uint8_t)sizeof(cmd));
        }
    }
    return map_xfer(res);
}

/* ---- Public API ---------------------------------------------------- */

esp_err_t rc522_init(const rc522_config_t *p_cfg)
{
    gpio_config_t rst = { 0 };
    spi_bus_config_t bus = { 0 };
    spi_device_interface_config_t dev = { 0 };
    uint8_t version;

    ESP_RETURN_ON_FALSE(NULL != p_cfg, ESP_ERR_INVALID_ARG, TAG, "no config");
    ESP_RETURN_ON_FALSE(NULL == s_dev, ESP_ERR_INVALID_STATE, TAG, "already initialised");

    rst.pin_bit_mask = 1ULL << p_cfg->rst;
    rst.mode         = GPIO_MODE_OUTPUT;
    ESP_RETURN_ON_ERROR(gpio_config(&rst), TAG, "rst pin");
    (void)gpio_set_level(p_cfg->rst, 0);
    vTaskDelay(pdMS_TO_TICKS(10u));
    (void)gpio_set_level(p_cfg->rst, 1);
    vTaskDelay(pdMS_TO_TICKS(50u));

    bus.sclk_io_num    = p_cfg->sck;
    bus.miso_io_num    = p_cfg->miso;
    bus.mosi_io_num    = p_cfg->mosi;
    bus.quadwp_io_num  = -1;
    bus.quadhd_io_num  = -1;
    dev.clock_speed_hz = p_cfg->clock_hz;
    dev.mode           = 0;
    dev.spics_io_num   = p_cfg->cs;
    dev.queue_size     = 1;
    ESP_RETURN_ON_ERROR(spi_bus_initialize(p_cfg->host, &bus, SPI_DMA_DISABLED), TAG, "bus");
    ESP_RETURN_ON_ERROR(spi_bus_add_device(p_cfg->host, &dev, &s_dev), TAG, "device");

    reg_write(REG_COMMAND, CMD_SOFT_RESET);
    vTaskDelay(pdMS_TO_TICKS(50u));
    version = reg_read(REG_VERSION);
    if ((0x00u == version) || (0xFFu == version))
    {
        ESP_LOGE(TAG, "no RC522 (VersionReg 0x%02X)", version);
        return ESP_ERR_NOT_FOUND;
    }

    reg_write(REG_T_MODE, 0x80u);        /* 25 ms answer timeout, TAuto */
    reg_write(REG_T_PRESCALER, 0xA9u);
    reg_write(REG_T_RELOAD_H, 0x03u);
    reg_write(REG_T_RELOAD_L, 0xE8u);
    reg_write(REG_TX_ASK, 0x40u);        /* 100 % ASK */
    reg_write(REG_MODE, 0x3Du);          /* CRC preset 0x6363 */
    reg_set_bits(REG_TX_CONTROL, 0x03u); /* antenna on */
    ESP_LOGI(TAG, "version 0x%02X, antenna on", version);
    return ESP_OK;
}

esp_err_t rc522_poll(rc522_tag_t *p_tag)
{
    esp_err_t err;

    ESP_RETURN_ON_FALSE((NULL != s_dev) && (NULL != p_tag), ESP_ERR_INVALID_STATE,
                        TAG, "not initialised");
    (void)memset(p_tag, 0, sizeof(*p_tag));
    err = wake_select(p_tag);
    if (ESP_ERR_NOT_FOUND != err)
    {
        end_session();
    }
    return err;
}

esp_err_t rc522_read_record(const rc522_tag_t *p_tag, uint8_t p_buf[RC522_RECORD_LEN])
{
    esp_err_t err;

    ESP_RETURN_ON_FALSE((NULL != s_dev) && (NULL != p_tag) && (NULL != p_buf),
                        ESP_ERR_INVALID_ARG, TAG, "args");
    err = reselect(p_tag);
    if (ESP_OK == err)
    {
        err = read_selected(p_tag, p_buf);
    }
    end_session();
    return err;
}

esp_err_t rc522_write_record(const rc522_tag_t *p_tag, const uint8_t p_buf[RC522_RECORD_LEN])
{
    uint8_t   back[RC522_RECORD_LEN];
    esp_err_t err;

    ESP_RETURN_ON_FALSE((NULL != s_dev) && (NULL != p_tag) && (NULL != p_buf),
                        ESP_ERR_INVALID_ARG, TAG, "args");
    err = reselect(p_tag);
    if (ESP_OK == err)
    {
        err = read_selected(p_tag, back);       /* also authenticates Classic */
    }
    if (ESP_OK == err)
    {
        err = write_selected(p_tag, p_buf);
    }
    if (ESP_OK == err)
    {
        /* verify: plain READ, still authenticated (Classic) */
        err = map_xfer(mf_read_block(is_classic(p_tag->sak) ? RECORD_BLOCK
                                                            : NTAG_FIRST_PAGE, back));
        if ((ESP_OK == err) && (0 != memcmp(back, p_buf, RC522_RECORD_LEN)))
        {
            err = ESP_ERR_INVALID_CRC;
        }
    }
    end_session();
    return err;
}
