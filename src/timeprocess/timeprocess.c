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

#define RTC_NODE DT_NODELABEL(rtc_ext)

static const struct device *const rtc_dev = DEVICE_DT_GET(RTC_NODE);

/* Protects the cached time from concurrent access. */
K_MUTEX_DEFINE(time_cache_mutex);

static struct rtc_time cached_time;
static bool cached_time_valid;

static bool timeprocess_is_leap_year(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

static int timeprocess_days_in_month(int year, int month)
{
    static const uint8_t days_per_month[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };

    if (month == 1 && timeprocess_is_leap_year(year)) {
        return 29;
    }

    return days_per_month[month];
}

static uint32_t timeprocess_days_before_year(int year)
{
    uint32_t days = 0;

    for (int current_year = 2000; current_year < year; current_year++) {
        days += timeprocess_is_leap_year(current_year) ? 366U : 365U;
    }

    return days;
}

static uint32_t timeprocess_days_before_month(int year, int month)
{
    uint32_t days = 0;

    for (int current_month = 0; current_month < month; current_month++) {
        days += (uint32_t)timeprocess_days_in_month(year, current_month);
    }

    return days;
}

static int timeprocess_validate_datetime(const struct rtc_time *time)
{
    if (time == NULL ||
        time->tm_year + 1900 < 2000 || time->tm_year + 1900 > 2099 ||
        time->tm_mon < 0 || time->tm_mon > 11 ||
        time->tm_mday < 1 ||
        time->tm_mday > timeprocess_days_in_month(time->tm_year + 1900,
                                                  time->tm_mon) ||
        time->tm_hour < 0 || time->tm_hour > 23 ||
        time->tm_min < 0 || time->tm_min > 59 ||
        time->tm_sec < 0 || time->tm_sec > 59) {
        return -EINVAL;
    }

    return 0;
}

static int timeprocess_datetime_to_seconds(const struct rtc_time *time,
                                           uint64_t *seconds)
{
    int ret = timeprocess_validate_datetime(time);
    if (ret != 0 || seconds == NULL) {
        return -EINVAL;
    }

    int year = time->tm_year + 1900;
    uint64_t days = timeprocess_days_before_year(year) +
                    timeprocess_days_before_month(year, time->tm_mon) +
                    (uint64_t)(time->tm_mday - 1);

    *seconds = days * 86400U + (uint64_t)time->tm_hour * 3600U +
               (uint64_t)time->tm_min * 60U + (uint64_t)time->tm_sec;

    return 0;
}

static void timeprocess_seconds_to_datetime(uint64_t seconds,
                                            struct rtc_time *time)
{
    uint32_t days = (uint32_t)(seconds / 86400U);
    uint32_t seconds_in_day = (uint32_t)(seconds % 86400U);
    int year = 2000;

    while (days >= (timeprocess_is_leap_year(year) ? 366U : 365U)) {
        days -= timeprocess_is_leap_year(year) ? 366U : 365U;
        year++;
    }

    int yday = (int)days;
    int month = 0;
    while (days >= (uint32_t)timeprocess_days_in_month(year, month)) {
        days -= (uint32_t)timeprocess_days_in_month(year, month);
        month++;
    }

    time->tm_year = year - 1900;
    time->tm_mon = month;
    time->tm_mday = (int)days + 1;
    time->tm_hour = (int)(seconds_in_day / 3600U);
    time->tm_min = (int)((seconds_in_day % 3600U) / 60U);
    time->tm_sec = (int)(seconds_in_day % 60U);
    time->tm_wday = (int)(((seconds / 86400U) + 6U) % 7U);
    time->tm_yday = yday;
    time->tm_isdst = -1;
    time->tm_nsec = 0;
}

static uint64_t timeprocess_dst_transition_utc(int year, int month)
{
    uint32_t days_before_month =
        timeprocess_days_before_year(year) +
        timeprocess_days_before_month(year, month);
    int days_in_month = timeprocess_days_in_month(year, month);
    int weekday_of_last_day =
        (int)(((uint64_t)days_before_month + (uint64_t)days_in_month - 1U +
               6U) % 7U);
    int last_sunday = days_in_month - weekday_of_last_day;

    return ((uint64_t)days_before_month + (uint64_t)(last_sunday - 1)) *
               86400U +
           3600U;
}

static bool timeprocess_is_summer_time_utc(const struct rtc_time *utc_time,
                                           uint64_t utc_seconds)
{
    int year = utc_time->tm_year + 1900;
    uint64_t summer_time_start = timeprocess_dst_transition_utc(year, 2);
    uint64_t summer_time_end = timeprocess_dst_transition_utc(year, 9);

    return utc_seconds >= summer_time_start && utc_seconds < summer_time_end;
}

static int timeprocess_local_to_utc(const struct rtc_time *local_time,
                                    struct rtc_time *utc_time)
{
    int ret = timeprocess_validate_datetime(local_time);
    if (ret != 0 || utc_time == NULL) {
        return -EINVAL;
    }

    int year = local_time->tm_year + 1900;
    int march_last_sunday =
        timeprocess_days_in_month(year, 2) -
        (int)(((uint64_t)timeprocess_days_before_year(year) +
               timeprocess_days_before_month(year, 2) +
               (uint64_t)timeprocess_days_in_month(year, 2) - 1U + 6U) %
              7U);

    if (local_time->tm_mon == 2 &&
        local_time->tm_mday == march_last_sunday &&
        local_time->tm_hour == 2) {
        return -EINVAL;
    }

    bool summer_time = false;
    if (local_time->tm_mon > 2 && local_time->tm_mon < 9) {
        summer_time = true;
    } else if (local_time->tm_mon == 2) {
        summer_time = local_time->tm_mday > march_last_sunday ||
                      (local_time->tm_mday == march_last_sunday &&
                       local_time->tm_hour >= 3);
    } else if (local_time->tm_mon == 9) {
        int october_last_sunday =
            timeprocess_days_in_month(year, 9) -
            (int)(((uint64_t)timeprocess_days_before_year(year) +
                   timeprocess_days_before_month(year, 9) +
                   (uint64_t)timeprocess_days_in_month(year, 9) - 1U +
                   6U) %
                  7U);

        summer_time = local_time->tm_mday < october_last_sunday ||
                      (local_time->tm_mday == october_last_sunday &&
                       local_time->tm_hour < 3);
    }

    uint64_t local_seconds;
    ret = timeprocess_datetime_to_seconds(local_time, &local_seconds);
    if (ret != 0) {
        return ret;
    }

    uint64_t offset = summer_time ? 7200U : 3600U;
    if (local_seconds < offset) {
        return -EINVAL;
    }

    timeprocess_seconds_to_datetime(local_seconds - offset, utc_time);
    if (utc_time->tm_year + 1900 < 2000) {
        return -EINVAL;
    }

    return 0;
}

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

static int timeprocess_read_rtc(void)
{
    struct rtc_time utc_time;
    int ret = rtc_get_time(rtc_dev, &utc_time);

    if (ret == 0) {
        /* The DS3231 driver exposes its register's one-based month and weekday. */
        utc_time.tm_mon--;
        if (utc_time.tm_wday >= 1 && utc_time.tm_wday <= 7) {
            utc_time.tm_wday = (utc_time.tm_wday + 6) % 7;
        }

        uint64_t utc_seconds;
        ret = timeprocess_datetime_to_seconds(&utc_time, &utc_seconds);
        if (ret == 0) {
            bool summer_time =
                timeprocess_is_summer_time_utc(&utc_time, utc_seconds);
            struct rtc_time local_time;

            timeprocess_seconds_to_datetime(
                utc_seconds + (summer_time ? 7200U : 3600U), &local_time);
            local_time.tm_isdst = summer_time ? 1 : 0;
            timeprocess_cache_update(&local_time);
        }
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

    struct rtc_time utc_time;
    int ret = timeprocess_local_to_utc(time, &utc_time);
    if (ret != 0) {
        return ret;
    }

    /* The DS3231 driver writes these fields directly to one-based registers. */
    utc_time.tm_mon++;
    if (utc_time.tm_wday >= 0 && utc_time.tm_wday <= 6) {
        utc_time.tm_wday++;
    }

    ret = rtc_set_time(rtc_dev, &utc_time);

    if (ret == 0) {
        /* Refresh the cache immediately instead of waiting for the next thread cycle. */
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
        shell_error(sh, "Could not read the RTC (error %d).", ret);
        return ret;
    }

    ret = timeprocess_get_time(&time);
    if (ret < 0) {
        shell_error(sh, "No valid time value is available.");
        return ret;
    }

    shell_print(sh, "Local time: %04d-%02d-%02d %02d:%02d.",
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
        shell_error(sh, "Usage: time set YYYY-MM-DD HH:MM.");
        return -EINVAL;
    }

    int year, month, day;
    int hour, minute;
    char dummy_date, dummy_time;

    int scanned;

    scanned = sscanf(argv[1], "%d-%d-%d%c", &year, &month, &day, &dummy_date);
    if (scanned != 3) {
        shell_error(sh, "Invalid date format. Usage: time set YYYY-MM-DD HH:MM.");
        return -EINVAL;
    }

    scanned = sscanf(argv[2], "%d:%d%c", &hour, &minute, &dummy_time);
    if (scanned != 2) {
        shell_error(sh, "Invalid time format. Usage: time set YYYY-MM-DD HH:MM.");
        return -EINVAL;
    }

    /* The DS3231 supports years from 2000 through 2099. */
    if (year < 2000 || year > 2099 ||
        month < 1 || month > 12 ||
        day < 1 || day > 31 ||
        hour < 0 || hour > 23 ||
        minute < 0 || minute > 59) {
        shell_error(sh, "The date or time is out of range.");
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
        shell_error(sh, "Could not set the local time (error %d).",
                    ret);
        return ret;
    }

    shell_print(sh, "Local time set to %04d-%02d-%02d %02d:%02d.",
                year, month, day, hour, minute);

    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
    rtc_commands,
    SHELL_CMD(get, NULL, "Read date and time", cmd_rtc_get),
    SHELL_CMD(set, NULL, "Set date and time (YYYY-MM-DD HH:MM)", cmd_rtc_set),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(time, &rtc_commands, "Date and time controls", NULL);

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

    LOG_INF("Time-processing thread started.");

    if (!device_is_ready(rtc_dev)) {
        LOG_ERR("DS3231 RTC is not ready.");
        return;
    }

    while (true) {
        int ret = timeprocess_read_rtc();

        if (ret < 0) {
            LOG_WRN("Could not read the RTC (error %d).", ret);
        } else {
            struct rtc_time time;

            ret = timeprocess_get_time(&time);
            if (ret < 0) {
                LOG_ERR("Could not read the time cache (error %d).", ret);
            } else if (time.tm_hour < 0 || time.tm_hour > 23 ||
                       time.tm_min < 0 || time.tm_min > 59 ||
                       time.tm_sec < 0 || time.tm_sec > 59) {
                LOG_ERR("RTC returned an invalid time (%02d:%02d:%02d).",
                        time.tm_hour, time.tm_min, time.tm_sec);
            } else {
                uint8_t slot = (uint8_t)((time.tm_min * 60 + time.tm_sec) / 150);

                if (time.tm_hour != last_hour || slot != last_slot) {
                    ret = timeprocess_render_time(time.tm_hour, slot);
                    if (ret < 0) {
                        LOG_ERR("Could not display the time (error %d).", ret);
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
