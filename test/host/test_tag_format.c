#include <string.h>
#include "test.h"
#include "tag_format.h"

TEST(parses_valid_record)
{
    const uint8_t buf[16] = {0x53, 0x42, 0x01, 7, 200};
    tag_record_t  rec     = {0};

    CHECK_EQ(TAG_OK, tag_parse(buf, sizeof(buf), &rec));
    CHECK_EQ(7, rec.story_id);
    CHECK_EQ(200, rec.hue);
}

TEST(rejects_foreign_tag)
{
    const uint8_t buf[16] = {0x04, 0xA1, 0x01, 7, 200}; /* e.g. bank card */
    tag_record_t  rec     = {0};

    CHECK_EQ(TAG_ERR_MAGIC, tag_parse(buf, sizeof(buf), &rec));
}

TEST(rejects_blank_tag)
{
    const uint8_t buf[16] = {0};
    tag_record_t  rec     = {0};

    CHECK_EQ(TAG_ERR_MAGIC, tag_parse(buf, sizeof(buf), &rec));
}

TEST(rejects_unknown_version)
{
    const uint8_t buf[16] = {0x53, 0x42, 0x02, 7, 200};
    tag_record_t  rec     = {0};

    CHECK_EQ(TAG_ERR_VERSION, tag_parse(buf, sizeof(buf), &rec));
}

TEST(rejects_story_zero)
{
    const uint8_t buf[16] = {0x53, 0x42, 0x01, 0, 200};
    tag_record_t  rec     = {0};

    CHECK_EQ(TAG_ERR_STORY, tag_parse(buf, sizeof(buf), &rec));
}

TEST(rejects_short_buffer)
{
    const uint8_t buf[4] = {0x53, 0x42, 0x01, 7};
    tag_record_t  rec    = {0};

    CHECK_EQ(TAG_ERR_LEN, tag_parse(buf, sizeof(buf), &rec));
}

TEST(error_leaves_record_unchanged)
{
    const uint8_t buf[16] = {0};
    tag_record_t  rec     = {.story_id = 9, .hue = 9};

    (void)tag_parse(buf, sizeof(buf), &rec);
    CHECK_EQ(9, rec.story_id);
    CHECK_EQ(9, rec.hue);
}

TEST(null_arguments)
{
    uint8_t      buf[16] = {0};
    tag_record_t rec     = {0};

    CHECK_EQ(TAG_ERR_ARG, tag_parse(NULL, sizeof(buf), &rec));
    CHECK_EQ(TAG_ERR_ARG, tag_parse(buf, sizeof(buf), NULL));
    CHECK_EQ(TAG_ERR_ARG, tag_build(NULL, buf, sizeof(buf)));
    CHECK_EQ(TAG_ERR_ARG, tag_build(&rec, NULL, sizeof(buf)));
}

TEST(build_pads_with_zeros_and_round_trips)
{
    const tag_record_t in      = {.story_id = 12, .hue = 34};
    tag_record_t       out     = {0};
    uint8_t            buf[16];

    (void)memset(buf, 0xFF, sizeof(buf));
    CHECK_EQ(TAG_OK, tag_build(&in, buf, sizeof(buf)));
    for (size_t i = TAG_RECORD_LEN; i < sizeof(buf); i++)
    {
        CHECK_EQ(0, buf[i]);
    }
    CHECK_EQ(TAG_OK, tag_parse(buf, sizeof(buf), &out));
    CHECK_EQ(12, out.story_id);
    CHECK_EQ(34, out.hue);
}

TEST(build_rejects_story_zero)
{
    const tag_record_t in = {.story_id = 0, .hue = 1};
    uint8_t            buf[16];

    CHECK_EQ(TAG_ERR_STORY, tag_build(&in, buf, sizeof(buf)));
}

static void run_suite(void)
{
    RUN(parses_valid_record);
    RUN(rejects_foreign_tag);
    RUN(rejects_blank_tag);
    RUN(rejects_unknown_version);
    RUN(rejects_story_zero);
    RUN(rejects_short_buffer);
    RUN(error_leaves_record_unchanged);
    RUN(null_arguments);
    RUN(build_pads_with_zeros_and_round_trips);
    RUN(build_rejects_story_zero);
}
