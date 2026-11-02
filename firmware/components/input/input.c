/**
 * @file  input.c
 * @brief Button and encoder input (see input.h for the contract).
 *
 * Proven in bring-up 10: 4 counts per click, lossless fast spin,
 * 30 ms debounce enough for the arcade button's microswitch.
 */

#include "input.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/pulse_cnt.h"
#include "esp_check.h"
#include "esp_log.h"

#define PCNT_LIMIT   (1000)
#define GLITCH_NS    (10000u)    /* PCNT max on ESP32 ~12.7 us */
#define TASK_STACK   (3072u)

typedef struct
{
    gpio_num_t pin;
    bool       b_pressed;   /* debounced state */
    uint32_t   differ;      /* consecutive samples that disagree with it */
} button_t;

static const char        *TAG = "input";
static QueueHandle_t      s_queue;
static pcnt_unit_handle_t s_pcnt;
static input_config_t     s_cfg;

/* ---- Encoder: PCNT quadrature x4 ------------------------------------ */

static esp_err_t encoder_init(void)
{
    const pcnt_unit_config_t unit_cfg =
    {
        .low_limit  = -PCNT_LIMIT,
        .high_limit = PCNT_LIMIT,
        .flags      = { .accum_count = 1 },
    };
    const pcnt_glitch_filter_config_t filt_cfg = { .max_glitch_ns = GLITCH_NS };
    const pcnt_chan_config_t a_cfg =
    {
        .edge_gpio_num  = s_cfg.enc_a,
        .level_gpio_num = s_cfg.enc_b,
    };
    const pcnt_chan_config_t b_cfg =
    {
        .edge_gpio_num  = s_cfg.enc_b,
        .level_gpio_num = s_cfg.enc_a,
    };
    pcnt_channel_handle_t ch_a;
    pcnt_channel_handle_t ch_b;

    ESP_RETURN_ON_ERROR(pcnt_new_unit(&unit_cfg, &s_pcnt), TAG, "pcnt unit");
    ESP_RETURN_ON_ERROR(pcnt_unit_set_glitch_filter(s_pcnt, &filt_cfg), TAG, "filter");
    ESP_RETURN_ON_ERROR(pcnt_new_channel(s_pcnt, &a_cfg, &ch_a), TAG, "chan a");
    ESP_RETURN_ON_ERROR(pcnt_new_channel(s_pcnt, &b_cfg, &ch_b), TAG, "chan b");

    ESP_RETURN_ON_ERROR(pcnt_channel_set_edge_action(ch_a,
        PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE), TAG, "a edge");
    ESP_RETURN_ON_ERROR(pcnt_channel_set_level_action(ch_a,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE), TAG, "a level");
    ESP_RETURN_ON_ERROR(pcnt_channel_set_edge_action(ch_b,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE), TAG, "b edge");
    ESP_RETURN_ON_ERROR(pcnt_channel_set_level_action(ch_b,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE), TAG, "b level");

    /* Watch points at the limits make accum_count work */
    ESP_RETURN_ON_ERROR(pcnt_unit_add_watch_point(s_pcnt, PCNT_LIMIT), TAG, "wp+");
    ESP_RETURN_ON_ERROR(pcnt_unit_add_watch_point(s_pcnt, -PCNT_LIMIT), TAG, "wp-");

    ESP_RETURN_ON_ERROR(pcnt_unit_enable(s_pcnt), TAG, "enable");
    ESP_RETURN_ON_ERROR(pcnt_unit_clear_count(s_pcnt), TAG, "clear");
    return pcnt_unit_start(s_pcnt);
}

/** Whole detents since the last call; the remainder stays in *p_ref. */
static int encoder_steps(int *p_ref)
{
    int count = 0;
    int steps = 0;

    (void)pcnt_unit_get_count(s_pcnt, &count);
    while ((count - *p_ref) >= s_cfg.counts_per_step)
    {
        *p_ref += s_cfg.counts_per_step;
        steps++;
    }
    while ((*p_ref - count) >= s_cfg.counts_per_step)
    {
        *p_ref -= s_cfg.counts_per_step;
        steps--;
    }
    return steps;
}

/* ---- Buttons: polled, debounced ------------------------------------ */

/** @return true once per press (debounced, on the press edge). */
static bool button_pressed(button_t *p_btn)
{
    const bool b_raw   = (0 == gpio_get_level(p_btn->pin));   /* active low */
    bool       b_event = false;

    if (b_raw == p_btn->b_pressed)
    {
        p_btn->differ = 0u;
    }
    else if (++p_btn->differ >= INPUT_DEBOUNCE_SAMPLES)
    {
        p_btn->b_pressed = b_raw;
        p_btn->differ    = 0u;
        b_event          = b_raw;
    }
    else
    {
        /* still bouncing / not yet stable */
    }
    return b_event;
}

static void post_press(input_evt_type_t type)
{
    const input_event_t evt = { .type = type, .steps = 0 };

    if (pdTRUE != xQueueSend(s_queue, &evt, 0))
    {
        ESP_LOGW(TAG, "queue full, press %d dropped", (int)type);
    }
}

/* ---- Task ----------------------------------------------------------- */

static void input_task(void *p_arg)
{
    button_t   lamp    = { .pin = s_cfg.lamp_btn };
    button_t   enc_sw  = { .pin = s_cfg.enc_sw };
    int        ref     = 0;    /* PCNT count at the last whole detent */
    int        pending = 0;    /* steps not yet in the queue */
    TickType_t wake    = xTaskGetTickCount();

    (void)p_arg;

    for (;;)
    {
        if (button_pressed(&lamp))
        {
            post_press(INPUT_EVT_LAMP_PRESS);
        }
        if (button_pressed(&enc_sw))
        {
            post_press(INPUT_EVT_ENC_PRESS);
        }

        /* Accumulate, then post as one event if there is room.
         * Clamp so the sum always fits int8_t. */
        pending += encoder_steps(&ref);
        pending  = (pending > INT8_MAX) ? INT8_MAX :
                   ((pending < INT8_MIN) ? INT8_MIN : pending);
        if (0 != pending)
        {
            const input_event_t evt =
            {
                .type  = INPUT_EVT_ENC_STEP,
                .steps = (int8_t)pending,
            };

            if (pdTRUE == xQueueSend(s_queue, &evt, 0))
            {
                pending = 0;
            }
        }

        /* Fixed period, independent of how long the loop body took */
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(INPUT_POLL_MS));
    }
}

/* ---- Public API ----------------------------------------------------- */

esp_err_t input_init(const input_config_t *p_cfg)
{
    gpio_config_t btn_cfg = { 0 };

    ESP_RETURN_ON_FALSE(NULL != p_cfg, ESP_ERR_INVALID_ARG, TAG, "no config");
    ESP_RETURN_ON_FALSE(p_cfg->counts_per_step > 0, ESP_ERR_INVALID_ARG, TAG,
                        "counts_per_step");
    ESP_RETURN_ON_FALSE(NULL == s_queue, ESP_ERR_INVALID_STATE, TAG,
                        "already initialised");
    s_cfg = *p_cfg;

    btn_cfg.pin_bit_mask = (1ULL << s_cfg.lamp_btn) | (1ULL << s_cfg.enc_sw);
    btn_cfg.mode         = GPIO_MODE_INPUT;
    btn_cfg.pull_up_en   = GPIO_PULLUP_ENABLE;
    ESP_RETURN_ON_ERROR(gpio_config(&btn_cfg), TAG, "buttons");
    ESP_RETURN_ON_ERROR(encoder_init(), TAG, "encoder");

    s_queue = xQueueCreate(INPUT_QUEUE_DEPTH, sizeof(input_event_t));
    ESP_RETURN_ON_FALSE(NULL != s_queue, ESP_ERR_NO_MEM, TAG, "queue");
    ESP_RETURN_ON_FALSE(pdPASS == xTaskCreate(input_task, "input", TASK_STACK,
                                              NULL, s_cfg.task_prio, NULL),
                        ESP_ERR_NO_MEM, TAG, "task");
    return ESP_OK;
}

bool input_get_event(input_event_t *p_evt, uint32_t timeout_ms)
{
    if ((NULL == s_queue) || (NULL == p_evt))
    {
        return false;
    }
    return pdTRUE == xQueueReceive(s_queue, p_evt, pdMS_TO_TICKS(timeout_ms));
}
