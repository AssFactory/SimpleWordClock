#ifndef TIME_FRAME_H
#define TIME_FRAME_H

#include <stdint.h>

#define TIME_FRAME_WIDTH 12U
#define TIME_FRAME_HEIGHT 10U

int time_frame_build(
    int hour, uint8_t slot,
    uint8_t frame[TIME_FRAME_HEIGHT][TIME_FRAME_WIDTH]);

#endif
