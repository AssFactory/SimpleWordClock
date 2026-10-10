#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <zephyr/drivers/led_strip.h>

int display_init(void);

int display_request_update(void);

int display_led_test(void);

int display_set_pixel(uint8_t x, uint8_t y, struct led_rgb color);

#endif
