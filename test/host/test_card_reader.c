/*
 * Executable specification for card_reader (ADR 0004, F9, F13).
 */

#include <string.h>
#include "test.h"
#include "card_reader.h"
#include "tag_format.h"

static card_reader_t s_rd;

/* Fake reader: returns s_block, or fails while s_fail_left > 0 */
static uint8_t  s_block[CARD_READER_BLOCK_LEN];
static uint32_t s_fail_left;
static uint32_t s_reads;

static bool fake_read(void *p_ctx, uint8_t p_buf[CARD_READER_BLOCK_LEN])
{
    (void)p_ctx;
    s_reads++;
    if (s_fail_left > 0u)
    {
        s_fail_left--;
        return false;
    }
    (void)memcpy(p_buf, s_block, CARD_READER_BLOCK_LEN);
    return true;
}

static const uint8_t UID_A[4] = { 0xA1u, 0xA2u, 0xA3u, 0xA4u };
static const uint8_t UID_B[4] = { 0xB1u, 0xB2u, 0xB3u, 0xB4u };

static void setup(uint8_t story)
{
    const tag_record_t rec = { .story_id = story, .hue = 99u };

    card_reader_init(&s_rd);
    (void)tag_build(&rec, s_block, sizeof(s_block));
    s_fail_left = 0u;
    s_reads     = 0u;
}

static card_poll_t tag(const uint8_t *p_uid)
{
    return card_reader_step(&s_rd, CARD_RD_TAG, p_uid, 4u, fake_read, NULL);
}

static card_poll_t status(card_rd_t st)
{
    return card_reader_step(&s_rd, st, NULL, 0u, fake_read, NULL);
}

TEST(no_tag_is_absent)
{
    setup(7u);
    CHECK(!status(CARD_RD_NONE).b_present);
}

TEST(valid_card_is_read_once_and_cached)
{
    card_poll_t p;

    setup(7u);
    p = tag(UID_A);
    CHECK(p.b_present && p.b_valid);
    CHECK_EQ(7, p.story_id);
    CHECK_EQ(99, p.hue);
    p = tag(UID_A);
    p = tag(UID_A);
    CHECK(p.b_valid);
    CHECK_EQ(1, s_reads);
}

TEST(card_removed_and_back_is_read_again)
{
    setup(7u);
    (void)tag(UID_A);
    (void)status(CARD_RD_NONE);
    (void)tag(UID_A);
    CHECK_EQ(2, s_reads);
}

TEST(other_card_is_read)
{
    setup(7u);
    (void)tag(UID_A);
    (void)tag(UID_B);
    CHECK_EQ(2, s_reads);
}

TEST(foreign_record_is_invalid_without_retries)
{
    card_poll_t p;

    setup(7u);
    (void)memset(s_block, 0xEEu, sizeof(s_block));
    p = tag(UID_A);
    CHECK(p.b_present && !p.b_valid);
    CHECK_EQ(4, p.uid_len);
    (void)tag(UID_A);
    CHECK_EQ(1, s_reads);
}

TEST(collision_is_present_invalid_without_uid)
{
    card_poll_t p;

    setup(7u);
    p = status(CARD_RD_COLLISION);
    CHECK(p.b_present && !p.b_valid);
    CHECK_EQ(0, p.uid_len);
}

TEST(garbled_poll_repeats_last_output)
{
    card_poll_t p;

    setup(7u);
    (void)tag(UID_A);
    p = status(CARD_RD_GARBLED);
    CHECK(p.b_present && p.b_valid);
    CHECK_EQ(7, p.story_id);
}

TEST(garbled_after_nothing_stays_absent)
{
    setup(7u);
    CHECK(!status(CARD_RD_GARBLED).b_present);
}

TEST(read_failure_never_reports_invalid_before_retries)
{
    card_poll_t p;

    setup(7u);
    s_fail_left = CARD_READER_RETRIES - 1u;
    for (uint32_t i = 0u; i < (CARD_READER_RETRIES - 1u); i++)
    {
        p = tag(UID_A);
        CHECK(!p.b_present);            /* repeats "absent", no chime */
    }
    p = tag(UID_A);                     /* read succeeds now */
    CHECK(p.b_valid);
    CHECK_EQ(7, p.story_id);
}

TEST(read_failure_while_playing_keeps_story)
{
    card_poll_t p;

    setup(7u);
    (void)tag(UID_A);
    (void)status(CARD_RD_NONE);         /* lifted */
    s_fail_left = 1u;
    p = tag(UID_A);                     /* back, but read fails once */
    CHECK(!p.b_present);                /* last output repeated: absent */
    p = tag(UID_A);
    CHECK(p.b_valid);
}

TEST(unreadable_card_becomes_invalid_after_retries)
{
    card_poll_t p = { 0 };

    setup(7u);
    s_fail_left = 100u;
    for (uint32_t i = 0u; i < CARD_READER_RETRIES; i++)
    {
        p = tag(UID_A);
    }
    CHECK(p.b_present && !p.b_valid);
    CHECK_EQ(CARD_READER_RETRIES, s_reads);
    (void)tag(UID_A);                   /* cached as invalid: no more reads */
    CHECK_EQ(CARD_READER_RETRIES, s_reads);
}

TEST(retry_count_restarts_for_another_uid)
{
    card_poll_t p;

    setup(7u);
    s_fail_left = CARD_READER_RETRIES - 1u;
    (void)tag(UID_A);
    (void)tag(UID_A);                   /* A failed twice */
    s_fail_left = 1u;
    p = tag(UID_B);                     /* B fails once: not invalid yet */
    CHECK(!(p.b_present && !p.b_valid && (4u == p.uid_len) &&
            (0 == memcmp(p.uid, UID_B, 4u))));
}

TEST(oversized_uid_is_treated_as_garbled)
{
    uint8_t     big[CARD_UID_MAX_LEN + 1u] = { 0 };
    card_poll_t p;

    setup(7u);
    p = card_reader_step(&s_rd, CARD_RD_TAG, big, (uint8_t)sizeof(big),
                         fake_read, NULL);
    CHECK(!p.b_present);
    CHECK_EQ(0, s_reads);
}

static void run_suite(void)
{
    RUN(no_tag_is_absent);
    RUN(valid_card_is_read_once_and_cached);
    RUN(card_removed_and_back_is_read_again);
    RUN(other_card_is_read);
    RUN(foreign_record_is_invalid_without_retries);
    RUN(collision_is_present_invalid_without_uid);
    RUN(garbled_poll_repeats_last_output);
    RUN(garbled_after_nothing_stays_absent);
    RUN(read_failure_never_reports_invalid_before_retries);
    RUN(read_failure_while_playing_keeps_story);
    RUN(unreadable_card_becomes_invalid_after_retries);
    RUN(retry_count_restarts_for_another_uid);
    RUN(oversized_uid_is_treated_as_garbled);
}
