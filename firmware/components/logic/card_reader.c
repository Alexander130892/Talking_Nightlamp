/**
 * @file  card_reader.c
 * @brief Record cache and read retries (see card_reader.h).
 */

#include "card_reader.h"

#include <string.h>
#include "tag_format.h"

static bool same_uid(const uint8_t *p_a, uint8_t a_len,
                     const uint8_t *p_b, uint8_t b_len)
{
    return (a_len == b_len) && (0 == memcmp(p_a, p_b, a_len));
}

void card_reader_init(card_reader_t *p_rd)
{
    (void)memset(p_rd, 0, sizeof(*p_rd));
}

static card_poll_t new_tag(card_reader_t *p_rd, const uint8_t *p_uid,
                           uint8_t uid_len, card_read_fn_t read_fn, void *p_ctx)
{
    uint8_t     buf[CARD_READER_BLOCK_LEN];
    card_poll_t out = { 0 };

    out.b_present = true;
    out.uid_len   = uid_len;
    (void)memcpy(out.uid, p_uid, uid_len);

    if (!same_uid(p_rd->fail_uid, p_rd->fail_uid_len, p_uid, uid_len))
    {
        p_rd->fails        = 0u;
        p_rd->fail_uid_len = uid_len;
        (void)memcpy(p_rd->fail_uid, p_uid, uid_len);
    }

    if ((NULL != read_fn) && read_fn(p_ctx, buf))
    {
        tag_record_t rec = { 0 };

        if (TAG_OK == tag_parse(buf, sizeof(buf), &rec))
        {
            out.b_valid  = true;
            out.story_id = rec.story_id;
            out.hue      = rec.hue;
        }
    }
    else if (++p_rd->fails < CARD_READER_RETRIES)
    {
        return p_rd->last;              /* no information: retry next poll */
    }
    else
    {
        /* unreadable after retries: a card with another key -> invalid */
    }

    p_rd->fails    = 0u;
    p_rd->b_cached = true;
    p_rd->cached   = out;
    return out;
}

card_poll_t card_reader_step(card_reader_t *p_rd, card_rd_t status,
                             const uint8_t *p_uid, uint8_t uid_len,
                             card_read_fn_t read_fn, void *p_ctx)
{
    card_poll_t out = { 0 };

    if (uid_len > CARD_UID_MAX_LEN)
    {
        status = CARD_RD_GARBLED;
    }

    switch (status)
    {
        case CARD_RD_NONE:
            p_rd->b_cached     = false;
            p_rd->fails        = 0u;
            p_rd->fail_uid_len = 0u;
            break;                              /* out: absent */

        case CARD_RD_COLLISION:
            out.b_present = true;               /* invalid, uid_len 0 */
            break;

        case CARD_RD_TAG:
            if (p_rd->b_cached &&
                same_uid(p_rd->cached.uid, p_rd->cached.uid_len, p_uid, uid_len))
            {
                out = p_rd->cached;
            }
            else
            {
                out = new_tag(p_rd, p_uid, uid_len, read_fn, p_ctx);
            }
            break;

        case CARD_RD_GARBLED:
        default:
            out = p_rd->last;                   /* no information */
            break;
    }

    p_rd->last = out;
    return out;
}
