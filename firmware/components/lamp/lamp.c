/**
 * @file  lamp.c
 * @brief Lamp task (see lamp.h).
 */

#include "lamp.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lamp_anim.h"
#include "lamp_model.h"
#include "lamp_tint.h"
#include "power_limit.h"

#define TASK_STACK (3072u)
#define MAX_PIXELS (64u)

typedef enum
{
    CMD_FADE,
    CMD_PULSE,
    CMD_BREATHE,
    CMD_TINT,
} cmd_type_t;

typedef struct
{
    cmd_type_t type;
    uint16_t   level_pm;      /* FADE */
    uint32_t   dur_ms;        /* FADE */
    bool       b_on;          /* BREATHE */
    uint8_t    hue;           /* TINT */
    uint16_t   strength_pm;   /* TINT */
} cmd_t;

static const char      *TAG = "lamp";
static QueueHandle_t    s_cmds;
static size_t           s_n;
static volatile uint16_t s_base_pm;   /* published for lamp_get_level() */

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void post(const cmd_t *p_cmd)
{
    if ((NULL == s_cmds) || (pdTRUE != xQueueSend(s_cmds, p_cmd, 0)))
    {
        ESP_LOGW(TAG, "command %d dropped", (int)p_cmd->type);
    }
}

static void lamp_task(void *p_arg)
{
    static pixel_grbw_t frame[MAX_PIXELS];
    lamp_anim_t         anim;
    uint8_t             hue         = 0u;
    uint16_t            strength_pm = 0u;
    TickType_t          wake        = xTaskGetTickCount();
    cmd_t               cmd;

    (void)p_arg;
    lamp_anim_init(&anim);

    for (;;)
    {
        const uint32_t t = now_ms();

        /* Apply every pending command, never block */
        while (pdTRUE == xQueueReceive(s_cmds, &cmd, 0))
        {
            switch (cmd.type)
            {
                case CMD_FADE:    lamp_anim_fade_to(&anim, cmd.level_pm, cmd.dur_ms, t); break;
                case CMD_PULSE:   lamp_anim_pulse(&anim, t);                             break;
                case CMD_BREATHE: lamp_anim_breathe(&anim, cmd.b_on, t);                 break;
                case CMD_TINT:    hue = cmd.hue; strength_pm = cmd.strength_pm;          break;
                default:                                                                 break;
            }
        }

        lamp_model_render(lamp_anim_level(&anim, t), frame, s_n);
        lamp_tint_apply(frame, s_n, hue, strength_pm);
        power_limit_apply(frame, s_n, POWER_LIMIT_BUDGET_MA);
        (void)lamp_strip_show(frame, s_n);
        s_base_pm = lamp_anim_base(&anim, t);

        vTaskDelayUntil(&wake, pdMS_TO_TICKS(LAMP_FRAME_MS));
    }
}

esp_err_t lamp_start(const lamp_config_t *p_cfg)
{
    ESP_RETURN_ON_FALSE(NULL != p_cfg, ESP_ERR_INVALID_ARG, TAG, "no config");
    ESP_RETURN_ON_FALSE(p_cfg->strip.n_pixels <= MAX_PIXELS, ESP_ERR_INVALID_ARG,
                        TAG, "n_pixels");
    ESP_RETURN_ON_FALSE(NULL == s_cmds, ESP_ERR_INVALID_STATE, TAG, "already started");

    ESP_RETURN_ON_ERROR(lamp_strip_init(&p_cfg->strip), TAG, "strip");
    s_n    = p_cfg->strip.n_pixels;
    s_cmds = xQueueCreate(LAMP_CMD_QUEUE_DEPTH, sizeof(cmd_t));
    ESP_RETURN_ON_FALSE(NULL != s_cmds, ESP_ERR_NO_MEM, TAG, "queue");
    ESP_RETURN_ON_FALSE(pdPASS == xTaskCreate(lamp_task, "lamp", TASK_STACK, NULL,
                                              p_cfg->task_prio, NULL),
                        ESP_ERR_NO_MEM, TAG, "task");
    return ESP_OK;
}

void lamp_fade_to(uint16_t level_pm, uint32_t dur_ms)
{
    const cmd_t cmd = { .type = CMD_FADE, .level_pm = level_pm, .dur_ms = dur_ms };

    post(&cmd);
}

void lamp_pulse(void)
{
    const cmd_t cmd = { .type = CMD_PULSE };

    post(&cmd);
}

void lamp_breathe(bool b_on)
{
    const cmd_t cmd = { .type = CMD_BREATHE, .b_on = b_on };

    post(&cmd);
}

void lamp_set_tint(uint8_t hue, uint16_t strength_pm)
{
    const cmd_t cmd = { .type = CMD_TINT, .hue = hue, .strength_pm = strength_pm };

    post(&cmd);
}

uint16_t lamp_get_level(void)
{
    return s_base_pm;
}
