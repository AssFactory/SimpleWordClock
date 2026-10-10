#include<stdint.h>
#ifndef DISPLAY_H
#define DISPLAY_H

int display_init(void);

int display_set_brightness(uint8_t brightness);

int display_led_test(void);


#endif