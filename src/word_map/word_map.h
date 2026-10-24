#ifndef WORD_MAP_H
#define WORD_MAP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Startkoordinate (x, y) und Wortlaenge in LEDs. */
struct word_coord {
    uint8_t x;
    uint8_t y;
    uint8_t len;
};

enum word_id {
    WORD_S,
    WORD_ISCH,
    WORD_GLEI,
    WORD_FEMF,
    WORD_ZEHN,
    WORD_NACH,
    WORD_VOR,
    WORD_VIERTEL,
    WORD_HALBER,
    WORD_DREIVIERTEL,

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
    WORD_ZWELFE,

    WORD_COUNT
};

/* Gibt bei ungueltiger ID NULL zurueck. */
const struct word_coord *word_map_get(enum word_id id);

#ifdef __cplusplus
}
#endif

#endif /* WORD_MAP_H */
