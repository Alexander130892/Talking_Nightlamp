/**
 * @file  power_limit.c
 * @brief LED current limiter.
 *
 * TODO: implement. Specification: power_limit.h and
 * test/host/test_power_limit.c (run with `make -C test/host spec`).
 */

#include "power_limit.h"

uint32_t power_limit_estimate_ma(const pixel_grbw_t *p_frame, size_t n)
{
    (void)p_frame;
    (void)n;

    return 0u;
}

void power_limit_apply(pixel_grbw_t *p_frame, size_t n, uint32_t budget_ma)
{
    (void)p_frame;
    (void)n;
    (void)budget_ma;
}
