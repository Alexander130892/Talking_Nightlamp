/**
 * @file  power_limit.c
 * @brief LED current limiter (ADR 0008, docs/hardware/power-budget.md).
 *
 * Model: each channel draws value / 255 * POWER_LIMIT_MA_PER_CHANNEL.
 * The frame's current is therefore proportional to the sum of all channel
 * values, and scaling every channel by the same factor scales the current
 * by that factor while keeping colour ratios.
 *
 * Specification: test/host/test_power_limit.c
 */

#include "power_limit.h"

#define CHANNEL_MAX (255u)

/** Sum of all channel values. Max 4 * 255 per pixel: no overflow for any
 *  realistic strip length (uint32_t holds > 4 million pixels). */
static uint32_t channel_sum(const pixel_grbw_t *p_frame, size_t n)
{
    uint32_t sum = 0u;

    for (size_t i = 0u; i < n; i++)
    {
        sum += (uint32_t)p_frame[i].g;
        sum += (uint32_t)p_frame[i].r;
        sum += (uint32_t)p_frame[i].b;
        sum += (uint32_t)p_frame[i].w;
    }
    return sum;
}

/** v * num / den, rounded down. Callers guarantee den > 0 and the result
 *  <= v (num < den), so it always fits in a uint8_t. */
static uint8_t scale(uint8_t v, uint32_t num, uint32_t den)
{
    return (uint8_t)(((uint32_t)v * num) / den);
}

uint32_t power_limit_estimate_ma(const pixel_grbw_t *p_frame, size_t n)
{
    const uint32_t sum = channel_sum(p_frame, n);

    /* ceil(sum * mA / 255): round up, err on the safe side */
    return ((sum * POWER_LIMIT_MA_PER_CHANNEL) + (CHANNEL_MAX - 1u)) /
           CHANNEL_MAX;
}

void power_limit_apply(pixel_grbw_t *p_frame, size_t n, uint32_t budget_ma)
{
    const uint32_t sum = channel_sum(p_frame, n);
    uint32_t       num;
    uint32_t       den;

    if (power_limit_estimate_ma(p_frame, n) <= budget_ma)
    {
        return; /* within budget: frame unchanged */
    }

    /* k = budget / estimate = (budget * 255) / (sum * mA_per_channel).
     * Over budget implies sum > 0, so den > 0, and num < den, so k < 1. */
    num = budget_ma * CHANNEL_MAX;
    den = sum * POWER_LIMIT_MA_PER_CHANNEL;

    for (size_t i = 0u; i < n; i++)
    {
        p_frame[i].g = scale(p_frame[i].g, num, den);
        p_frame[i].r = scale(p_frame[i].r, num, den);
        p_frame[i].b = scale(p_frame[i].b, num, den);
        p_frame[i].w = scale(p_frame[i].w, num, den);
    }
}