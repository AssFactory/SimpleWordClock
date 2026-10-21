#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "timeprocess.h"
#include "../display/display.h"

LOG_MODULE_REGISTER(timeprocess, LOG_LEVEL_INF);

/* Devicetree-Label des DS3231-RTC-Unterknotens */
#define RTC_NODE DT_NODELABEL(rtc_ext)

static const struct device *const rtc_dev = DEVICE_DT_GET(RTC_NODE);

/* Schützt den zusammengehörigen Zeitstand vor parallelem Zugriff. */
K_MUTEX_DEFINE(time_cache_mutex);

static struct rtc_time cached_time;
static bool cached_time_valid;

/* Übernimmt einen Zeitstand sicher in den Cache. */
static void timeprocess_cache_update(const struct rtc_time *time)
{
    k_mutex_lock(&time_cache_mutex, K_FOREVER);
    cached_time = *time;
    cached_time_valid = true;
    k_mutex_unlock(&time_cache_mutex);
}

int timeprocess_get_time(struct rtc_time *time)
{
    if (time == NULL) {
        return -EINVAL;
    }

    k_mutex_lock(&time_cache_mutex, K_FOREVER);

    if (!cached_time_valid) {
        k_mutex_unlock(&time_cache_mutex);
        return -ENODATA;
    }

    *time = cached_time;
    k_mutex_unlock(&time_cache_mutex);

    return 0;
}

/* Liest die RTC direkt und aktualisiert anschließend den Cache. */
static int timeprocess_read_rtc(void)
{
    struct rtc_time time;
    int ret = rtc_get_time(rtc_dev, &time);

    if (ret == 0) {
        timeprocess_cache_update(&time);
    }

    return ret;
}

int timeprocess_set_time(const struct rtc_time *time)
{
    if (time == NULL) {
        return -EINVAL;
    }

    if (!device_is_ready(rtc_dev)) {
        return -ENODEV;
    }

    int ret = rtc_set_time(rtc_dev, time);

    if (ret == 0) {
        /* Cache sofort aktualisieren, ohne den nächsten Thread-Zyklus abzuwarten. */
        ret = timeprocess_read_rtc();
    }

    return ret;
}

/* rtc get */
static int cmd_rtc_get(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    struct rtc_time time;
    int ret = timeprocess_read_rtc();

    if (ret < 0) {
        shell_error(sh, "RTC lesen fehlgeschlagen: %d", ret);
        return ret;
    }

    ret = timeprocess_get_time(&time);
    if (ret < 0) {
        shell_error(sh, "Kein gueltiger Zeitwert verfuegbar.");
        return ret;
    }

    shell_print(sh, "%04d-%02d-%02d %02d:%02d",
                time.tm_year + 1900,
                time.tm_mon + 1,
                time.tm_mday,
                time.tm_hour,
                time.tm_min);

    return 0;
}

/* rtc set YYYY-MM-DD HH:MM */
/* rtc set YYYY-MM-DD HH:MM */
static int cmd_rtc_set(const struct shell *sh, size_t argc, char **argv)
{
    if (argc != 3) {
        shell_error(sh, "Verwendung: rtc set YYYY-MM-DD HH:MM");
        return -EINVAL;
    }

    int year, month, day;
    int hour, minute;
    char dummy_date, dummy_time;

    /* Wir versuchen, die Werte einzulesen UND ein unerwünschtes Nachfolge-Zeichen (dummy) mitzulesen.
     * Wenn sscanf GENAU 3 Werte beim Datum und GENAU 2 Werte bei der Zeit liefert 
     * UND KEIN zusätzliches Zeichen am Ende existiert, schlägt der zweite Teil fehl 
     * ODER wir prüfen die Anzahl der scanf-Erfolge. 
     * Besser und stabiler: Wir prüfen, ob sscanf genau die Anzahl liefert 
     * und nutzen einen Punkt/Leerzeichen oder prüfen das Ende direkt. */

    // Sauberer Ansatz ohne %n:
    // Wir prüfen mit sscanf ob die Zahlen stimmen. Um Müll am Ende abzufangen, 
    // hängen wir ein %c an und erwarten, dass sscanf DIESES ZEICHEN NICHT finden kann 
    // (sprich: der String ist hier zu Ende und sscanf gibt für diesen Teil 0 zurück).
    
    int scanned;

    // Datum prüfen: 3 Integers müssen kommen, danach darf nichts mehr kommen
    scanned = sscanf(argv[1], "%d-%d-%d%c", &year, &month, &day, &dummy_date);
    if (scanned != 3) {
        shell_error(sh, "Format-Fehler beim Datum: rtc set 2026-10-21 14:30");
        return -EINVAL;
    }

    // Zeit prüfen: 2 Integers müssen kommen, danach darf nichts mehr kommen
    scanned = sscanf(argv[2], "%d:%d%c", &hour, &minute, &dummy_time);
    if (scanned != 2) {
        shell_error(sh, "Format-Fehler bei der Zeit: rtc set 2026-10-21 14:30");
        return -EINVAL;
    }

    /* DS3231 unterstützt hier den Bereich 2000 bis 2099. */
    if (year < 2000 || year > 2099 ||
        month < 1 || month > 12 ||
        day < 1 || day > 31 ||
        hour < 0 || hour > 23 ||
        minute < 0 || minute > 59) {
        shell_error(sh, "Datum oder Uhrzeit ausserhalb des gueltigen Bereichs.");
        return -EINVAL;
    }

    struct rtc_time time = {
        .tm_year = year - 1900,
        .tm_mon = month - 1,
        .tm_mday = day,
        .tm_hour = hour,
        .tm_min = minute,
        .tm_sec = 0,
        .tm_wday = -1,
        .tm_yday = -1,
        .tm_isdst = -1,
        .tm_nsec = 0,
    };

    int ret = timeprocess_set_time(&time);

    if (ret < 0) {
        shell_error(sh, "RTC setzen fehlgeschlagen: %d", ret);
        return ret;
    }

    shell_print(sh, "RTC gesetzt auf %04d-%02d-%02d %02d:%02d",
                year, month, day, hour, minute);

    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
    rtc_commands,
    SHELL_CMD(get, NULL, "Datum und Uhrzeit auslesen", cmd_rtc_get),
    SHELL_CMD(set, NULL, "Zeit setzen: set YYYY-MM-DD HH:MM", cmd_rtc_set),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(rtc, &rtc_commands, "RTC Datum und Uhrzeit", NULL);

void timeprocess_thread(void *arg1, void *arg2, void *arg3)
{
    const uint32_t interval_ms = *(const uint32_t *)arg1;

    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    LOG_INF("Start timeprocess thread");

    if (!device_is_ready(rtc_dev)) {
        LOG_ERR("DS3231 RTC ist nicht bereit");
        return;
    }

    while (true) {
        int ret = timeprocess_read_rtc();

        if (ret < 0) {
            LOG_WRN("RTC lesen fehlgeschlagen: %d", ret);
        }

        ret = display_request_update();
        if (ret < 0) {
            LOG_ERR("Display request failed: %d", ret);
        }

        k_msleep(interval_ms);
    }
}
