#include <errno.h>
#include <string.h>

#include <zephyr/ztest.h>

#include "time_process/time_frame.h"
#include "word_map/word_map.h"

struct expected_word {
    const char *name;
    struct word_coord coord;
};

static const struct expected_word expected_words[WORD_COUNT] = {
    [WORD_S] = { "S", { 3, 0, 1 } },
    [WORD_ISCH] = { "ISCH", { 5, 0, 4 } },
    [WORD_GLEI] = { "GLEI", { 2, 1, 4 } },
    [WORD_FEMF] = { "FEMF", { 0, 2, 4 } },
    [WORD_ZEHN] = { "ZEHN", { 7, 1, 4 } },
    [WORD_NACH] = { "NACH", { 5, 2, 4 } },
    [WORD_VOR] = { "VOR", { 9, 2, 3 } },
    [WORD_VIERTEL] = { "VIERTEL", { 4, 3, 7 } },
    [WORD_HALBER] = { "HALBER", { 0, 4, 6 } },
    [WORD_DREIVIERTEL] = { "DREIVIERTEL", { 0, 3, 11 } },
    [WORD_OISE] = { "OISE", { 0, 6, 4 } },
    [WORD_ZWOIE] = { "ZWOIE", { 7, 4, 5 } },
    [WORD_DREIE] = { "DREIE", { 0, 5, 5 } },
    [WORD_VIERE] = { "VIERE", { 1, 7, 5 } },
    [WORD_FEMFE] = { "FEMFE", { 5, 8, 5 } },
    [WORD_SEGGSE] = { "SEGGSE", { 3, 9, 6 } },
    [WORD_SIEBENE] = { "SIEBENE", { 5, 5, 7 } },
    [WORD_ACHDE] = { "ACHDE", { 6, 7, 5 } },
    [WORD_NEINE] = { "NEINE", { 7, 6, 5 } },
    [WORD_ZEHNE] = { "ZEHNE", { 4, 6, 5 } },
    [WORD_ELFE] = { "ELFE", { 3, 8, 4 } },
    [WORD_ZWELFE] = { "ZWELFE", { 1, 8, 6 } },
};

struct expected_phrase {
    uint8_t count;
    enum word_id words[4];
};

static const struct expected_phrase expected_phrases[24] = {
    [0] = { 0, { 0 } },
    [1] = { 3, { WORD_GLEI, WORD_FEMF, WORD_NACH } },
    [2] = { 2, { WORD_FEMF, WORD_NACH } },
    [3] = { 3, { WORD_GLEI, WORD_ZEHN, WORD_NACH } },
    [4] = { 2, { WORD_ZEHN, WORD_NACH } },
    [5] = { 2, { WORD_GLEI, WORD_VIERTEL } },
    [6] = { 1, { WORD_VIERTEL } },
    [7] = { 4, { WORD_GLEI, WORD_ZEHN, WORD_VOR, WORD_HALBER } },
    [8] = { 3, { WORD_ZEHN, WORD_VOR, WORD_HALBER } },
    [9] = { 4, { WORD_GLEI, WORD_FEMF, WORD_VOR, WORD_HALBER } },
    [10] = { 3, { WORD_FEMF, WORD_VOR, WORD_HALBER } },
    [11] = { 2, { WORD_GLEI, WORD_HALBER } },
    [12] = { 1, { WORD_HALBER } },
    [13] = { 4, { WORD_GLEI, WORD_FEMF, WORD_NACH, WORD_HALBER } },
    [14] = { 3, { WORD_FEMF, WORD_NACH, WORD_HALBER } },
    [15] = { 4, { WORD_GLEI, WORD_ZEHN, WORD_NACH, WORD_HALBER } },
    [16] = { 3, { WORD_ZEHN, WORD_NACH, WORD_HALBER } },
    [17] = { 2, { WORD_GLEI, WORD_DREIVIERTEL } },
    [18] = { 1, { WORD_DREIVIERTEL } },
    [19] = { 3, { WORD_GLEI, WORD_ZEHN, WORD_VOR } },
    [20] = { 2, { WORD_ZEHN, WORD_VOR } },
    [21] = { 3, { WORD_GLEI, WORD_FEMF, WORD_VOR } },
    [22] = { 2, { WORD_FEMF, WORD_VOR } },
    [23] = { 1, { WORD_GLEI } },
};

static const enum word_id expected_hour_words[12] = {
    WORD_ZWELFE, WORD_OISE, WORD_ZWOIE, WORD_DREIE,
    WORD_VIERE, WORD_FEMFE, WORD_SEGGSE, WORD_SIEBENE,
    WORD_ACHDE, WORD_NEINE, WORD_ZEHNE, WORD_ELFE,
};

static void expected_add_word(
    enum word_id id, uint8_t frame[TIME_FRAME_HEIGHT][TIME_FRAME_WIDTH])
{
    const struct word_coord *coord = &expected_words[id].coord;

    for (uint8_t offset = 0; offset < coord->len; offset++) {
        frame[coord->y][coord->x + offset] = 1U;
    }
}

ZTEST_SUITE(word_map, NULL, NULL, NULL, NULL, NULL);

ZTEST(word_map, test_all_word_names_and_coordinates)
{
    zassert_equal(WORD_COUNT, ARRAY_SIZE(expected_words),
                  "Update expected words when the word map changes");

    for (int id = 0; id < WORD_COUNT; id++) {
        const struct word_coord *coord =
            word_map_get((enum word_id)id);
        const char *name = word_map_get_name((enum word_id)id);
        const struct expected_word *expected = &expected_words[id];

        zassert_not_null(coord, "Word %d has no coordinates", id);
        zassert_not_null(name, "Word %d has no name", id);
        zassert_equal(strcmp(name, expected->name), 0,
                      "Unexpected name for word %d", id);
        zassert_equal(coord->x, expected->coord.x,
                      "Unexpected x coordinate for %s", name);
        zassert_equal(coord->y, expected->coord.y,
                      "Unexpected y coordinate for %s", name);
        zassert_equal(coord->len, expected->coord.len,
                      "Unexpected length for %s", name);
        zassert_true(coord->len > 0U, "%s has zero length", name);
        zassert_true(coord->x + coord->len <= TIME_FRAME_WIDTH,
                     "%s exceeds the display width", name);
        zassert_true(coord->y < TIME_FRAME_HEIGHT,
                     "%s exceeds the display height", name);
    }
}

ZTEST(word_map, test_invalid_word_ids)
{
    zassert_is_null(word_map_get((enum word_id)-1),
                    "Negative word ID should be rejected");
    zassert_is_null(word_map_get(WORD_COUNT),
                    "Word-count ID should be rejected");
    zassert_is_null(word_map_get_name((enum word_id)-1),
                    "Negative word ID should not have a name");
    zassert_is_null(word_map_get_name(WORD_COUNT),
                    "Word-count ID should not have a name");
}

ZTEST_SUITE(time_frame, NULL, NULL, NULL, NULL, NULL);

ZTEST(time_frame, test_all_hours_and_minute_slots)
{
    uint8_t actual[TIME_FRAME_HEIGHT][TIME_FRAME_WIDTH];
    uint8_t expected[TIME_FRAME_HEIGHT][TIME_FRAME_WIDTH];

    for (uint8_t slot = 0; slot < ARRAY_SIZE(expected_phrases); slot++) {
        for (int hour = 0; hour < 24; hour++) {
            memset(expected, 0, sizeof(expected));
            expected_add_word(WORD_S, expected);
            expected_add_word(WORD_ISCH, expected);

            const struct expected_phrase *phrase = &expected_phrases[slot];
            for (uint8_t i = 0; i < phrase->count; i++) {
                expected_add_word(phrase->words[i], expected);
            }

            uint8_t display_hour = (uint8_t)(hour % 12);
            if (slot >= 5U) {
                display_hour = (uint8_t)((display_hour + 1U) % 12U);
            }
            expected_add_word(expected_hour_words[display_hour], expected);

            zassert_equal(time_frame_build(hour, slot, actual), 0,
                          "Failed to build frame for hour %d, slot %u",
                          hour, slot);
            zassert_mem_equal(actual, expected, sizeof(actual),
                              "Wrong frame for hour %d, slot %u", hour, slot);
        }
    }
}

ZTEST(time_frame, test_rejects_invalid_input)
{
    uint8_t frame[TIME_FRAME_HEIGHT][TIME_FRAME_WIDTH];

    zassert_equal(time_frame_build(-1, 0, frame), -EINVAL);
    zassert_equal(time_frame_build(24, 0, frame), -EINVAL);
    zassert_equal(time_frame_build(12, 24, frame), -EINVAL);
    zassert_equal(time_frame_build(12, 0, NULL), -EINVAL);
}
