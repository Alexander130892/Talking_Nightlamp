/**
 * @file  lamp_model.c
 * @brief Brightness model.
 *
 * TODO: implement. Specification: lamp_model.h and
 * test/host/test_lamp_model.c (run with `make -C test/host spec`).
 */

#include <string.h>
#include "lamp_model.h"

void lamp_model_render(uint16_t level_pm, pixel_grbw_t *p_frame, size_t n)
{
    (void)level_pm;
    (void)memset(p_frame, 0, n * sizeof(*p_frame));
}
