#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <stdint.h>

#include "ambiente.h"
#include "../display/display.h"

LOG_MODULE_REGISTER(ambiente, LOG_LEVEL_INF);

void ambiente_thread(void *arg1, void *arg2, void *arg3)
{
    const uint32_t interval_ms = *(const uint32_t *)arg1;

    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    LOG_INF("start ambiente thread");

    while (true) {
        /*
         * TODO:
         * Read the BH1750 light sensor.
         * Calculate the desired display brightness.
         */

        int ret = display_request_update();

        if (ret < 0) {
            printk("ambiente: Display request failed: %d\n", ret);
        }

        k_msleep(interval_ms);
    }
}

uint16_t ambiente_get_lux(void)
{

    return 0; 
}