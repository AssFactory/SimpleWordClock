#include "time_frame.h"

#include <errno.h>
#include <string.h>

#include "../word_map/word_map.h"

struct time_phrase {
    uint8_t count;
    enum word_id words[4];
};

static const struct time_phrase time_phrases[24] = {
    [0]  = { .count = 0 },
    [1]  = { .count = 3, .words = { WORD_GLEI, WORD_FEMF, WORD_NACH } },
    [2]  = { .count = 2, .words = { WORD_FEMF, WORD_NACH } },
    [3]  = { .count = 3, .words = { WORD_GLEI, WORD_ZEHN, WORD_NACH } },
    [4]  = { .count = 2, .words = { WORD_ZEHN, WORD_NACH } },
    [5]  = { .count = 2, .words = { WORD_GLEI, WORD_VIERTEL } },
    [6]  = { .count = 1, .words = { WORD_VIERTEL } },
    [7]  = { .count = 4, .words = { WORD_GLEI, WORD_ZEHN, WORD_VOR, WORD_HALBER } },
    [8]  = { .count = 3, .words = { WORD_ZEHN, WORD_VOR, WORD_HALBER } },
    [9]  = { .count = 4, .words = { WORD_GLEI, WORD_FEMF, WORD_VOR, WORD_HALBER } },
    [10] = { .count = 3, .words = { WORD_FEMF, WORD_VOR, WORD_HALBER } },
    [11] = { .count = 2, .words = { WORD_GLEI, WORD_HALBER } },
    [12] = { .count = 1, .words = { WORD_HALBER } },
    [13] = { .count = 4, .words = { WORD_GLEI, WORD_FEMF, WORD_NACH, WORD_HALBER } },
    [14] = { .count = 3, .words = { WORD_FEMF, WORD_NACH, WORD_HALBER } },
    [15] = { .count = 4, .words = { WORD_GLEI, WORD_ZEHN, WORD_NACH, WORD_HALBER } },
    [16] = { .count = 3, .words = { WORD_ZEHN, WORD_NACH, WORD_HALBER } },
    [17] = { .count = 2, .words = { WORD_GLEI, WORD_DREIVIERTEL } },
    [18] = { .count = 1, .words = { WORD_DREIVIERTEL } },
    [19] = { .count = 3, .words = { WORD_GLEI, WORD_ZEHN, WORD_VOR } },
    [20] = { .count = 2, .words = { WORD_ZEHN, WORD_VOR } },
    [21] = { .count = 3, .words = { WORD_GLEI, WORD_FEMF, WORD_VOR } },
    [22] = { .count = 2, .words = { WORD_FEMF, WORD_VOR } },
    [23] = { .count = 1, .words = { WORD_GLEI } },
};

static const enum word_id hour_words[12] = {
    WORD_ZWELFE,
    WORD_OISE,
    WORD_ZWOIE,
    WORD_DREIE,
    WORD_VIERE,
    WORD_FEMFE,
    WORD_SEGGSE,
    WORD_SIEBENE,
    WORD_ACHDE,
    WORD_NEINE,
    WORD_ZEHNE,
    WORD_ELFE,
};

static int time_frame_add_word(
    enum word_id id, uint8_t frame[TIME_FRAME_HEIGHT][TIME_FRAME_WIDTH])
{
    const struct word_coord *coord = word_map_get(id);
    if (coord == NULL || coord->x + coord->len > TIME_FRAME_WIDTH ||
        coord->y >= TIME_FRAME_HEIGHT) {
        return -EINVAL;
    }

    for (uint8_t offset = 0; offset < coord->len; offset++) {
        frame[coord->y][coord->x + offset] = 1U;
    }

    return 0;
}

int time_frame_build(
    int hour, uint8_t slot,
    uint8_t frame[TIME_FRAME_HEIGHT][TIME_FRAME_WIDTH])
{
    if (frame == NULL || hour < 0 || hour > 23 || slot >= 24U) {
        return -EINVAL;
    }

    memset(frame, 0, sizeof(uint8_t) * TIME_FRAME_HEIGHT * TIME_FRAME_WIDTH);

    int ret = time_frame_add_word(WORD_S, frame);
    if (ret == 0) {
        ret = time_frame_add_word(WORD_ISCH, frame);
    }

    const struct time_phrase *phrase = &time_phrases[slot];
    for (uint8_t i = 0; ret == 0 && i < phrase->count; i++) {
        ret = time_frame_add_word(phrase->words[i], frame);
    }

    uint8_t display_hour = (uint8_t)(hour % 12);
    if (slot >= 5U) {
        display_hour = (uint8_t)((display_hour + 1U) % 12U);
    }

    if (ret == 0) {
        ret = time_frame_add_word(hour_words[display_hour], frame);
    }

    return ret;
}
