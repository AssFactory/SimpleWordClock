
#include <zephyr/kernel.h>
#include <stdint.h>

#include "timeprocess.h"
#include "../display/display.h"

void timeprocess_thread(void *arg1, void *arg2, void *arg3)
{
    const uint32_t interval_ms = *(const uint32_t *)arg1;

    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    while (true) {
        /*
         * TODO:
         * Read the current time from the RTC.
         * Process the time into the corresponding words.
         */

        int ret = display_request_update();

        if (ret < 0) {
            printk("timeprocess: Display request failed: %d\n", ret);
        }

        k_msleep(interval_ms);
    }
}
