#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <zephyr/drivers/led_strip.h>

enum display_color {
    DISPLAY_COLOR_WHITE,
    DISPLAY_COLOR_RED,
    DISPLAY_COLOR_BLUE,
    DISPLAY_COLOR_GREEN,
    DISPLAY_COLOR_CYAN,
    DISPLAY_COLOR_YELLOW,
    DISPLAY_COLOR_MAGENTA,
    DISPLAY_COLOR_COUNT
};

int display_init(void);

int display_request_update(void);

int display_set_color(enum display_color color);

int display_get_color(enum display_color *color);

const char *display_color_name(enum display_color color);

int display_led_test(void);

int display_clear_buffer(void);

uint8_t display_scale_brightness(uint16_t lux_val);

int display_get_brightness_limits(uint8_t *min, uint8_t *max);

int display_set_brightness_limits(uint8_t min, uint8_t max);

int display_set_pixel_level(uint8_t x, uint8_t y, uint8_t level);

int display_set_pixel(uint8_t x, uint8_t y, struct led_rgb color);

#endif
