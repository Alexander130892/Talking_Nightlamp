/**
 * @file  card_fsm.c
 * @brief Card presence state machine.
 *
 * TODO: implement. Specification: card_fsm.h and test/host/test_card_fsm.c
 * (run with `make -C test/host spec`).
 */

#include <string.h>
#include "card_fsm.h"

void card_fsm_init(card_fsm_t *p_fsm, const card_fsm_cfg_t *p_cfg)
{
    (void)memset(p_fsm, 0, sizeof(*p_fsm));
    p_fsm->cfg   = *p_cfg;
    p_fsm->state = CARD_FSM_IDLE;
}

card_output_t card_fsm_step(card_fsm_t *p_fsm, const card_poll_t *p_poll,
                            uint32_t now_ms)
{
    card_output_t out = {CARD_ACT_NONE, 0u, 0u};

    (void)p_fsm;
    (void)p_poll;
    (void)now_ms;

    return out;
}

card_output_t card_fsm_story_finished(card_fsm_t *p_fsm, uint32_t now_ms)
{
    card_output_t out = {CARD_ACT_NONE, 0u, 0u};

    (void)p_fsm;
    (void)now_ms;

    return out;
}

card_fsm_state_t card_fsm_state(const card_fsm_t *p_fsm)
{
    return p_fsm->state;
}
