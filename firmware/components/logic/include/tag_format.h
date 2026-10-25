/**
 * @file  tag_format.h
 * @brief Story card record: 'S' 'B' version story_id hue.
 *
 * Format: docs/firmware/card-format.md. Rationale: ADR 0005.
 */

#ifndef TAG_FORMAT_H
#define TAG_FORMAT_H

#include <stddef.h>
#include <stdint.h>

#define TAG_RECORD_LEN      (5u)
#define TAG_MAGIC_0         (0x53u) /* 'S' */
#define TAG_MAGIC_1         (0x42u) /* 'B' */
#define TAG_FORMAT_VERSION  (0x01u)

typedef struct
{
    uint8_t story_id; /* 1..255, 0 is invalid */
    uint8_t hue;      /* lamp accent colour, 0..255 */
} tag_record_t;

typedef enum
{
    TAG_OK = 0,
    TAG_ERR_ARG,     /* NULL pointer */
    TAG_ERR_LEN,     /* buffer shorter than TAG_RECORD_LEN */
    TAG_ERR_MAGIC,   /* not one of our cards */
    TAG_ERR_VERSION, /* our card, unsupported format version */
    TAG_ERR_STORY    /* story_id == 0 */
} tag_status_t;

/**
 * Parse a record read from a tag. Bytes after the record are ignored.
 * On any error *p_rec is left unchanged.
 */
tag_status_t tag_parse(const uint8_t *p_buf, size_t len, tag_record_t *p_rec);

/**
 * Build a record for writing to a tag. Fills all len bytes; bytes after
 * the record are set to 0x00 (padding to the block/page size).
 */
tag_status_t tag_build(const tag_record_t *p_rec, uint8_t *p_buf, size_t len);

#endif /* TAG_FORMAT_H */
