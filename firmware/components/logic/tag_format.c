/**
 * @file  tag_format.c
 * @brief Story card record parser/builder.
 *
 * Implemented as a style reference for the other logic modules.
 */

#include <string.h>
#include "tag_format.h"

tag_status_t tag_parse(const uint8_t *p_buf, size_t len, tag_record_t *p_rec)
{
    tag_status_t status = TAG_OK;

    if ((NULL == p_buf) || (NULL == p_rec))
    {
        status = TAG_ERR_ARG;
    }
    else if (len < TAG_RECORD_LEN)
    {
        status = TAG_ERR_LEN;
    }
    else if ((TAG_MAGIC_0 != p_buf[0]) || (TAG_MAGIC_1 != p_buf[1]))
    {
        status = TAG_ERR_MAGIC;
    }
    else if (TAG_FORMAT_VERSION != p_buf[2])
    {
        status = TAG_ERR_VERSION;
    }
    else if (0u == p_buf[3])
    {
        status = TAG_ERR_STORY;
    }
    else
    {
        p_rec->story_id = p_buf[3];
        p_rec->hue      = p_buf[4];
    }

    return status;
}

tag_status_t tag_build(const tag_record_t *p_rec, uint8_t *p_buf, size_t len)
{
    tag_status_t status = TAG_OK;

    if ((NULL == p_buf) || (NULL == p_rec))
    {
        status = TAG_ERR_ARG;
    }
    else if (len < TAG_RECORD_LEN)
    {
        status = TAG_ERR_LEN;
    }
    else if (0u == p_rec->story_id)
    {
        status = TAG_ERR_STORY;
    }
    else
    {
        (void)memset(p_buf, 0, len);
        p_buf[0] = TAG_MAGIC_0;
        p_buf[1] = TAG_MAGIC_1;
        p_buf[2] = TAG_FORMAT_VERSION;
        p_buf[3] = p_rec->story_id;
        p_buf[4] = p_rec->hue;
    }

    return status;
}
