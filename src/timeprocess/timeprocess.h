#ifndef TIMEPROCESS_H
#define TIMEPROCESS_H

#include <zephyr/drivers/rtc.h>

void timeprocess_thread(void *arg1, void *arg2, void *arg3);

int timeprocess_get_time(struct rtc_time *time);
int timeprocess_set_time(const struct rtc_time *time);
int timeprocess_refresh_display(void);

#endif
