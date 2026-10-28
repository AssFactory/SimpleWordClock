#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/devicetree.h>

#include <stdint.h>
#include <errno.h>

#include "ambiente.h"
#include "../display/display.h"

LOG_MODULE_REGISTER(ambiente, LOG_LEVEL_INF);

#define AMBIENTE_BRIGHTNESS_CHANGE_THRESHOLD 2U

#define BH1750_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(rohm_bh1750)

static const struct device *const bh1750 =
    DEVICE_DT_GET(BH1750_NODE);

static atomic_t lux_value = ATOMIC_INIT(0);

static int ambiente_read_sensor(void)
{
    struct sensor_value value;

    if (!device_is_ready(bh1750)) {
        LOG_ERR("BH1750 is not ready");
        return -ENODEV;
    }

    int ret = sensor_sample_fetch(bh1750);
    if (ret < 0) {
        LOG_ERR("BH1750: Failed to fetch sample: %d", ret);
        return ret;
    }

    ret = sensor_channel_get(bh1750, SENSOR_CHAN_LIGHT, &value);
    if (ret < 0) {
        LOG_ERR("BH1750: Failed to read illuminance: %d", ret);
        return ret;
    }

    /* Round to the nearest lux and clamp negative readings to zero. */
    int32_t lux = value.val1;

    if (value.val2 >= 500000) {
        lux++;
    }

    if (lux < 0) {
        lux = 0;
    }

    atomic_set(&lux_value, (atomic_val_t)lux);

    LOG_DBG("Ambient illuminance: %d lx", (int)lux);

    return 0;
}

uint16_t ambiente_get_lux(void)
{
    atomic_val_t lux = atomic_get(&lux_value);

    if (lux < 0) {
        return 0;
    }

    if (lux > UINT16_MAX) {
        return UINT16_MAX;
    }

    return (uint16_t)lux;
}

static int cmd_lux(const struct shell *shell,
                   size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    shell_print(shell, "Ambient illuminance: %u lx",
                (unsigned int)ambiente_get_lux());

    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
    ambiente_commands,
    SHELL_CMD(get, NULL, "Get current illuminance", cmd_lux),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(ambiente, &ambiente_commands, "Ambient light sensor", NULL);

void ambiente_thread(void *arg1, void *arg2, void *arg3)
{
    const uint32_t interval_ms = *(const uint32_t *)arg1;
    uint8_t last_display_brightness = UINT8_MAX;

    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    LOG_INF("Ambient light thread started");

    if (!device_is_ready(bh1750)) {
        LOG_ERR("BH1750 is not ready");
        return;
    }

    while (true) {
        int ret = ambiente_read_sensor();
        if (ret == 0) {
            uint8_t brightness = display_scale_brightness(ambiente_get_lux());
            uint8_t difference = brightness > last_display_brightness
                                     ? brightness - last_display_brightness
                                     : last_display_brightness - brightness;

            if (last_display_brightness == UINT8_MAX ||
                difference >= AMBIENTE_BRIGHTNESS_CHANGE_THRESHOLD) {
                ret = display_request_update();
                if (ret < 0) {
                    LOG_ERR("Display update failed: %d", ret);
                } else {
                    last_display_brightness = brightness;
                }
            }
        }

        k_msleep(interval_ms);
    }
}
