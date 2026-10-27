#include "word_map.h"

#include <stddef.h>

static const struct word_coord word_coords[WORD_COUNT] = {
    [WORD_S]           = { 3, 0,  1 },
    [WORD_ISCH]        = { 5, 0,  4 },
    [WORD_GLEI]        = { 2, 1,  4 },
    [WORD_FEMF]        = { 0, 2,  4 },
    [WORD_ZEHN]        = { 7, 1,  4 },
    [WORD_NACH]        = { 5, 2,  4 },
    [WORD_VOR]         = { 9, 2,  3 },
    [WORD_VIERTEL]     = { 4, 3,  7 },
    [WORD_HALBER]      = { 0, 4,  6 },
    [WORD_DREIVIERTEL] = { 0, 3, 11 },

    [WORD_OISE]        = { 0, 6,  4 },
    [WORD_ZWOIE]       = { 7, 4,  5 },
    [WORD_DREIE]       = { 0, 5,  5 },
    [WORD_VIERE]       = { 1, 7,  5 },
    [WORD_FEMFE]       = { 5, 8,  5 },
    [WORD_SEGGSE]      = { 3, 9,  6 },
    [WORD_SIEBENE]     = { 5, 5,  7 },
    [WORD_ACHDE]       = { 6, 7,  5 },
    [WORD_NEINE]       = { 7, 6,  5 },
    [WORD_ZEHNE]       = { 4, 6,  5 },
    [WORD_ELFE]        = { 3, 8,  4 },
    [WORD_ZWELFE]      = { 1, 8,  6 },
};

const struct word_coord *word_map_get(enum word_id id)
{
    if ((unsigned int)id >= WORD_COUNT) {
        return NULL;
    }

    return &word_coords[id];
}
