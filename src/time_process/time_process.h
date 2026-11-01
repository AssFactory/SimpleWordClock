#ifndef TIME_PROCESS_H
#define TIME_PROCESS_H

#include <zephyr/drivers/rtc.h>

void time_process_thread(void *arg1, void *arg2, void *arg3);

int time_process_get_time(struct rtc_time *time);
/* Sets the RTC from local Central European time. */
int time_process_set_time(const struct rtc_time *time);
int time_process_refresh_display(void);

#endif
