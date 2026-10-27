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
#include "../word_map/word_map.h"

LOG_MODULE_REGISTER(timeprocess, LOG_LEVEL_INF);

struct time_phrase {
    uint8_t count;
    enum word_id words[4];
};

static const struct time_phrase time_phrases[24] = {
    [0]  = { .count = 0 },
    [1]  = { .count = 3, .words = { WORD_GLEI, WORD_FEMF, WORD_NACH} },
    [2]  = { .count = 2, .words = { WORD_FEMF, WORD_NACH } },
    [3]  = { .count = 3, .words = { WORD_GLEI, WORD_ZEHN, WORD_NACH } },
    [4]  = { .count = 2, .words = { WORD_ZEHN, WORD_NACH } },
    [5]  = { .count = 2, .words = { WORD_GLEI, WORD_VIERTEL } },
    [6]  = { .count = 1, .words = { WORD_VIERTEL } },
    [7]  = { .count = 4, .words = { WORD_GLEI, WORD_ZEHN, WORD_VOR, WORD_HALBER } },
    [8]  = { .count = 3, .words = { WORD_ZEHN, WORD_VOR,  WORD_HALBER } },
    [9]  = { .count = 4, .words = { WORD_GLEI, WORD_FEMF, WORD_VOR, WORD_HALBER } },
    [10] = { .count = 3, .words = { WORD_FEMF, WORD_VOR,  WORD_HALBER } },
    [11] = { .count = 2, .words = { WORD_GLEI, WORD_HALBER } },
    [12] = { .count = 1, .words = { WORD_HALBER } },
    [13] = { .count = 4, .words = { WORD_GLEI, WORD_FEMF, WORD_NACH, WORD_HALBER } },
    [14] = { .count = 3, .words = { WORD_FEMF, WORD_NACH, WORD_HALBER } },
    [15] = { .count = 4, .words = { WORD_GLEI, WORD_ZEHN, WORD_NACH, WORD_HALBER } },
    [16] = { .count = 3, .words = { WORD_ZEHN, WORD_NACH, WORD_HALBER } },
    [17] = { .count = 2, .words = { WORD_GLEI, WORD_DREIVIERTEL } },
    [18] = { .count = 1, .words = { WORD_DREIVIERTEL } },
    [19] = { .count = 3, .words = { WORD_GLEI, WORD_ZEHN, WORD_VOR } },
    [20] = { .count = 2, .words = { WORD_ZEHN, WORD_VOR } },
    [21] = { .count = 3, .words = { WORD_GLEI, WORD_FEMF, WORD_VOR } },
    [22] = { .count = 2, .words = { WORD_FEMF, WORD_VOR } },
    [23] = { .count = 1, .words = { WORD_GLEI } },
};

static const enum word_id hour_words[12] = {
    WORD_ZWELFE,
    WORD_OISE,
    WORD_ZWOIE,
    WORD_DREIE,
    WORD_VIERE,
    WORD_FEMFE,
    WORD_SEGGSE,
    WORD_SIEBENE,
    WORD_ACHDE,
    WORD_NEINE,
    WORD_ZEHNE,
    WORD_ELFE,
};

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


static int cmd_rtc_set(const struct shell *sh, size_t argc, char **argv)
{
    if (argc != 3) {
        shell_error(sh, "Verwendung: rtc set YYYY-MM-DD HH:MM");
        return -EINVAL;
    }

    int year, month, day;
    int hour, minute;
    char dummy_date, dummy_time;

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

SHELL_CMD_REGISTER(time, &rtc_commands, "RTC Datum und Uhrzeit", NULL);

static int timeprocess_set_word(enum word_id id)
{
    const struct word_coord *coord = word_map_get(id);
    if (coord == NULL) {
        return -EINVAL;
    }

    const struct led_rgb color = {
        .r = 255,
        .g = 255,
        .b = 255,
    };

    for (uint8_t offset = 0; offset < coord->len; offset++) {
        int ret = display_set_pixel(coord->x + offset, coord->y, color);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

static int timeprocess_render_time(int hour, uint8_t slot)
{
    const struct time_phrase *phrase = &time_phrases[slot];
    uint8_t display_hour = (uint8_t)(hour % 12);
    int ret = display_clear_buffer();

    if (ret != 0) {
        return ret;
    }

    ret = timeprocess_set_word(WORD_S);
    if (ret == 0) {
        ret = timeprocess_set_word(WORD_ISCH);
    }

    for (uint8_t i = 0; ret == 0 && i < phrase->count; i++) {
        ret = timeprocess_set_word(phrase->words[i]);
    }

    if (slot >= 5) {
        display_hour = (uint8_t)((display_hour + 1U) % 12U);
    }

    if (ret == 0) {
        ret = timeprocess_set_word(hour_words[display_hour]);
    }
    if (ret != 0) {
        return ret;
    }

    return display_request_update();
}

int timeprocess_refresh_display(void)
{
    struct rtc_time time;
    int ret = timeprocess_read_rtc();
    if (ret != 0) {
        return ret;
    }

    ret = timeprocess_get_time(&time);
    if (ret != 0) {
        return ret;
    }

    if (time.tm_hour < 0 || time.tm_hour > 23 ||
        time.tm_min < 0 || time.tm_min > 59 ||
        time.tm_sec < 0 || time.tm_sec > 59) {
        return -EINVAL;
    }

    uint8_t slot = (uint8_t)((time.tm_min * 60 + time.tm_sec) / 150);
    return timeprocess_render_time(time.tm_hour, slot);
}

void timeprocess_thread(void *arg1, void *arg2, void *arg3)
{
    const uint32_t interval_ms = *(const uint32_t *)arg1;
    int last_hour = -1;
    int last_slot = -1;

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
        } else {
            struct rtc_time time;

            ret = timeprocess_get_time(&time);
            if (ret < 0) {
                LOG_ERR("Zeit-Cache lesen fehlgeschlagen: %d", ret);
            } else if (time.tm_hour < 0 || time.tm_hour > 23 ||
                       time.tm_min < 0 || time.tm_min > 59 ||
                       time.tm_sec < 0 || time.tm_sec > 59) {
                LOG_ERR("RTC lieferte ungueltige Uhrzeit: %02d:%02d:%02d",
                        time.tm_hour, time.tm_min, time.tm_sec);
            } else {
                uint8_t slot = (uint8_t)((time.tm_min * 60 + time.tm_sec) / 150);

                if (time.tm_hour != last_hour || slot != last_slot) {
                    ret = timeprocess_render_time(time.tm_hour, slot);
                    if (ret < 0) {
                        LOG_ERR("Uhrzeit konnte nicht angezeigt werden: %d", ret);
                    } else {
                        last_hour = time.tm_hour;
                        last_slot = slot;
                    }
                }
            }
        }

        k_msleep(interval_ms);
    }
}
