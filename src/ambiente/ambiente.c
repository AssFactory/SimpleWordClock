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

/* BH1750 aus dem Devicetree */
#define BH1750_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(rohm_bh1750)

static const struct device *const bh1750 =
    DEVICE_DT_GET(BH1750_NODE);

/* Letzter gültiger Messwert in ganzen Lux */
static atomic_t lux_value = ATOMIC_INIT(0);

/*
 * BH1750 auslesen und den atomaren Messwert aktualisieren.
 * Bei einem Fehler bleibt der vorherige Wert erhalten.
 */
static int ambiente_read_sensor(void)
{
    struct sensor_value value;

    if (!device_is_ready(bh1750)) {
        LOG_ERR("BH1750 ist nicht bereit");
        return -ENODEV;
    }

    int ret = sensor_sample_fetch(bh1750);
    if (ret < 0) {
        LOG_ERR("BH1750: Messung fehlgeschlagen: %d", ret);
        return ret;
    }

    ret = sensor_channel_get(bh1750, SENSOR_CHAN_LIGHT, &value);
    if (ret < 0) {
        LOG_ERR("BH1750: Lux-Auslesen fehlgeschlagen: %d", ret);
        return ret;
    }

    /* Auf ganze Lux runden und negative Werte verhindern */
    int32_t lux = value.val1;

    if (value.val2 >= 500000) {
        lux++;
    }

    if (lux < 0) {
        lux = 0;
    }

    atomic_set(&lux_value, (atomic_val_t)lux);

    LOG_DBG("Umgebungshelligkeit: %d lx", (int)lux);

    return 0;
}

/*
 * Letzten gespeicherten Lux-Wert zurückgeben.
 * Sicher aus anderen Threads aufrufbar.
 */
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

/*
 * Shell: lux
 * Gibt den zuletzt gespeicherten Messwert aus.
 */
static int cmd_lux(const struct shell *shell,
                   size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    shell_print(shell, "Umgebungshelligkeit: %u lx",
                (unsigned int)ambiente_get_lux());

    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
    ambiente_commands,
    SHELL_CMD(get, NULL, "Get actual brightness", cmd_lux),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(ambiente, &ambiente_commands, "Ambiente functions", NULL);

/*
 * Bestehender Ambiente-Thread:
 * Sensor auslesen und Display-Update anfordern.
 */
void ambiente_thread(void *arg1, void *arg2, void *arg3)
{
    const uint32_t interval_ms = *(const uint32_t *)arg1;

    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    LOG_INF("Ambiente-Thread gestartet");

    if (!device_is_ready(bh1750)) {
        LOG_ERR("BH1750 ist nicht bereit");
        return;
    }

    while (true) {
        (void)ambiente_read_sensor();

        int ret = display_request_update();

        if (ret < 0) {
            LOG_ERR("Display-Update fehlgeschlagen: %d", ret);
        }

        k_msleep(interval_ms);
    }
}
