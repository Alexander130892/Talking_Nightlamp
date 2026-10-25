/**
 * @file  power_limit.h
 * @brief Estimate LED current for a frame and scale it to a budget.
 *
 * Rationale: ADR 0008, docs/hardware/power-budget.md.
 * Specification: test/host/test_power_limit.c.
 *
 * Model: every channel draws value / 255 * POWER_LIMIT_MA_PER_CHANNEL.
 *
 * Contract for power_limit_apply()
 *  - If the estimate is within budget, the frame is unchanged.
 *  - Otherwise every channel is scaled by the same factor (colour ratios
 *    preserved as far as 8-bit rounding allows) and the estimate of the
 *    result is <= budget_ma.
 *  - A channel is never increased.
 */

#ifndef POWER_LIMIT_H
#define POWER_LIMIT_H

#include <stddef.h>
#include <stdint.h>
#include "pixel.h"

#define POWER_LIMIT_MA_PER_CHANNEL (20u)
#define POWER_LIMIT_BUDGET_MA      (150u)

/** Estimated current in mA (rounded up). */
uint32_t power_limit_estimate_ma(const pixel_grbw_t *p_frame, size_t n);

/** Scale p_frame in place so its estimate is <= budget_ma. */
void power_limit_apply(pixel_grbw_t *p_frame, size_t n, uint32_t budget_ma);

#endif /* POWER_LIMIT_H */
