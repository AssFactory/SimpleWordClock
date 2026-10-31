#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/sys/util.h>

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

struct time_zone_config {
    int16_t utc_offset_minutes;
    bool dst_enabled;
};

struct time_zone_option {
    const char *name;
    int16_t utc_offset_minutes;
};

static const struct time_zone_option time_zone_options[] = {
    { "UTC-12:00", -720 },
    { "UTC-11:00", -660 },
    { "UTC-10:00", -600 },
    { "UTC-09:30", -570 },
    { "UTC-09:00", -540 },
    { "UTC-08:00", -480 },
    { "UTC-07:00", -420 },
    { "UTC-06:00", -360 },
    { "UTC-05:00", -300 },
    { "UTC-04:00", -240 },
    { "UTC-03:30", -210 },
    { "UTC-03:00", -180 },
    { "UTC-02:00", -120 },
    { "UTC-01:00", -60 },
    { "UTC+00:00", 0 },
    { "UTC+01:00", 60 },
    { "UTC+02:00", 120 },
    { "UTC+03:00", 180 },
    { "UTC+03:30", 210 },
    { "UTC+04:00", 240 },
    { "UTC+04:30", 270 },
    { "UTC+05:00", 300 },
    { "UTC+05:30", 330 },
    { "UTC+05:45", 345 },
    { "UTC+06:00", 360 },
    { "UTC+06:30", 390 },
    { "UTC+07:00", 420 },
    { "UTC+08:00", 480 },
    { "UTC+08:45", 525 },
    { "UTC+09:00", 540 },
    { "UTC+09:30", 570 },
    { "UTC+10:00", 600 },
    { "UTC+10:30", 630 },
    { "UTC+11:00", 660 },
    { "UTC+12:00", 720 },
    { "UTC+12:45", 765 },
    { "UTC+13:00", 780 },
    { "UTC+14:00", 840 },
};

static const struct time_zone_option *timeprocess_find_zone_by_offset(
    int16_t utc_offset_minutes)
{
    for (size_t i = 0; i < ARRAY_SIZE(time_zone_options); i++) {
        if (time_zone_options[i].utc_offset_minutes == utc_offset_minutes) {
            return &time_zone_options[i];
        }
    }

    return NULL;
}

static struct time_zone_config time_zone_config = {
    .utc_offset_minutes = 60,
    .dst_enabled = true,
};

K_MUTEX_DEFINE(time_zone_mutex);

/* Protects the cached time from concurrent access. */
K_MUTEX_DEFINE(time_cache_mutex);

static struct rtc_time cached_time;
static bool cached_time_valid;

#define TIME_DISPLAY_WIDTH 12U
#define TIME_DISPLAY_HEIGHT 10U
#define TIME_SWEEP_STEP_MS 33U
#define TIME_SWEEP_FADE_IN_FIRST 32U
#define TIME_SWEEP_FADE_IN_SECOND 64U
#define TIME_SWEEP_FADE_IN_THIRD 112U
#define TIME_SWEEP_FADE_IN_FOURTH 176U
#define TIME_SWEEP_FADE_IN_FIFTH 232U
#define TIME_SWEEP_FADE_OUT_FIRST 24U
#define TIME_SWEEP_FADE_OUT_SECOND 64U
#define TIME_SWEEP_FADE_OUT_THIRD 112U
#define TIME_SWEEP_FADE_OUT_FOURTH 176U
#define TIME_SWEEP_FADE_OUT_FIFTH 232U

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
                                           uint64_t utc_seconds,
                                           bool dst_enabled)
{
    if (!dst_enabled) {
        return false;
    }

    int year = utc_time->tm_year + 1900;
    uint64_t summer_time_start = timeprocess_dst_transition_utc(year, 2);
    uint64_t summer_time_end = timeprocess_dst_transition_utc(year, 9);

    return utc_seconds >= summer_time_start && utc_seconds < summer_time_end;
}

static struct time_zone_config timeprocess_get_zone_config(void)
{
    struct time_zone_config config;

    k_mutex_lock(&time_zone_mutex, K_FOREVER);
    config = time_zone_config;
    k_mutex_unlock(&time_zone_mutex);

    return config;
}

static int timeprocess_settings_set(const char *name, size_t len,
                                    settings_read_cb read_cb, void *cb_arg)
{
    if (strcmp(name, "zone") == 0) {
        int16_t utc_offset_minutes;

        if (len != sizeof(utc_offset_minutes)) {
            return -EINVAL;
        }

        ssize_t bytes_read = read_cb(cb_arg, &utc_offset_minutes,
                                     sizeof(utc_offset_minutes));
        if (bytes_read != sizeof(utc_offset_minutes)) {
            return bytes_read < 0 ? (int)bytes_read : -EIO;
        }

        if (timeprocess_find_zone_by_offset(utc_offset_minutes) == NULL) {
            LOG_WRN("Stored UTC offset %d is no longer supported; using UTC+01:00.",
                    utc_offset_minutes);
            return 0;
        }

        k_mutex_lock(&time_zone_mutex, K_FOREVER);
        time_zone_config.utc_offset_minutes = utc_offset_minutes;
        k_mutex_unlock(&time_zone_mutex);
        return 0;
    }

    if (strcmp(name, "dst") == 0) {
        uint8_t dst_enabled;

        if (len != sizeof(dst_enabled)) {
            return -EINVAL;
        }

        ssize_t bytes_read = read_cb(cb_arg, &dst_enabled,
                                     sizeof(dst_enabled));
        if (bytes_read != sizeof(dst_enabled)) {
            return bytes_read < 0 ? (int)bytes_read : -EIO;
        }

        if (dst_enabled > 1U) {
            return -EINVAL;
        }

        k_mutex_lock(&time_zone_mutex, K_FOREVER);
        time_zone_config.dst_enabled = dst_enabled != 0U;
        k_mutex_unlock(&time_zone_mutex);
        return 0;
    }

    return -ENOENT;
}

SETTINGS_STATIC_HANDLER_DEFINE(clock, "clock", NULL, timeprocess_settings_set,
                               NULL, NULL);

static int timeprocess_local_to_utc(const struct rtc_time *local_time,
                                    struct rtc_time *utc_time,
                                    const struct time_zone_config *config)
{
    int ret = timeprocess_validate_datetime(local_time);
    if (ret != 0 || utc_time == NULL || config == NULL) {
        return -EINVAL;
    }

    uint64_t local_seconds;
    ret = timeprocess_datetime_to_seconds(local_time, &local_seconds);
    if (ret != 0) {
        return ret;
    }

    int32_t standard_offset_seconds =
        (int32_t)config->utc_offset_minutes * 60;
    size_t offset_count = config->dst_enabled ? 2U : 1U;

    for (size_t i = 0; i < offset_count; i++) {
        bool candidate_is_dst = config->dst_enabled && i == 0U;
        int32_t offset = standard_offset_seconds +
                         (candidate_is_dst ? 3600 : 0);
        uint64_t utc_seconds;

        if (offset >= 0) {
            if (local_seconds < (uint32_t)offset) {
                continue;
            }
            utc_seconds = local_seconds - (uint32_t)offset;
        } else {
            utc_seconds = local_seconds + (uint32_t)(-offset);
        }

        struct rtc_time candidate_utc;
        timeprocess_seconds_to_datetime(utc_seconds, &candidate_utc);
        if (candidate_utc.tm_year + 1900 > 2099) {
            continue;
        }

        bool is_dst = timeprocess_is_summer_time_utc(
            &candidate_utc, utc_seconds, config->dst_enabled);
        if (is_dst == candidate_is_dst) {
            *utc_time = candidate_utc;
            return 0;
        }
    }

    return -EINVAL;
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
            struct time_zone_config config =
                timeprocess_get_zone_config();
            bool summer_time = timeprocess_is_summer_time_utc(
                &utc_time, utc_seconds, config.dst_enabled);
            struct rtc_time local_time;
            int32_t offset_seconds =
                (int32_t)config.utc_offset_minutes * 60 +
                (summer_time ? 3600 : 0);

            if (offset_seconds >= 0) {
                utc_seconds += (uint32_t)offset_seconds;
            } else {
                uint32_t offset = (uint32_t)(-offset_seconds);
                if (utc_seconds < offset) {
                    return -EINVAL;
                }
                utc_seconds -= offset;
            }

            timeprocess_seconds_to_datetime(utc_seconds, &local_time);
            if (local_time.tm_year + 1900 > 2099) {
                return -EINVAL;
            }
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
    struct time_zone_config config = timeprocess_get_zone_config();
    int ret = timeprocess_local_to_utc(time, &utc_time, &config);
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

static int cmd_time_zone_get(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    struct time_zone_config config = timeprocess_get_zone_config();
    const struct time_zone_option *zone =
        timeprocess_find_zone_by_offset(config.utc_offset_minutes);
    if (zone == NULL) {
        shell_error(sh, "The configured UTC offset is not supported.");
        return -EINVAL;
    }

    shell_print(sh, "%s.", zone->name);
    return 0;
}

static int cmd_time_zone_set(const struct shell *sh, size_t argc, char **argv)
{
    if (argc != 2U) {
        shell_error(sh, "Usage: time zone set UTC+01:00.");
        return -EINVAL;
    }

    const struct time_zone_option *zone = NULL;
    for (size_t i = 0; i < ARRAY_SIZE(time_zone_options); i++) {
        if (strcmp(argv[1], time_zone_options[i].name) == 0) {
            zone = &time_zone_options[i];
            break;
        }
    }

    if (zone == NULL) {
        shell_error(sh, "Unsupported UTC zone. Use 'time zone list' to list choices.");
        return -EINVAL;
    }

    int16_t stored_offset = zone->utc_offset_minutes;
    int ret = settings_save_one("clock/zone", &stored_offset,
                                sizeof(stored_offset));
    if (ret < 0) {
        shell_error(sh, "Could not save the UTC offset (error %d).", ret);
        return ret;
    }

    k_mutex_lock(&time_zone_mutex, K_FOREVER);
    time_zone_config.utc_offset_minutes = stored_offset;
    k_mutex_unlock(&time_zone_mutex);

    shell_print(sh, "Time zone set to %s.", zone->name);
    ret = timeprocess_refresh_display();
    if (ret < 0) {
        shell_error(sh, "Offset saved, but display refresh failed (error %d).",
                    ret);
    }

    return ret;
}

static int cmd_time_zone_list(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    shell_print(sh, "Available fixed UTC offsets:");
    for (size_t i = 0; i < ARRAY_SIZE(time_zone_options); i++) {
        shell_print(sh, "%s", time_zone_options[i].name);
    }

    return 0;
}

static int cmd_time_dst_get(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    struct time_zone_config config = timeprocess_get_zone_config();
    shell_print(sh, "EU daylight saving is %s.",
                config.dst_enabled ? "on" : "off");
    return 0;
}

static int cmd_time_dst_set(const struct shell *sh, size_t argc, char **argv)
{
    if (argc != 2U ||
        (strcmp(argv[1], "on") != 0 && strcmp(argv[1], "off") != 0)) {
        shell_error(sh, "Usage: time dst set <on|off>.");
        return -EINVAL;
    }

    uint8_t dst_enabled = strcmp(argv[1], "on") == 0 ? 1U : 0U;
    int ret = settings_save_one("clock/dst", &dst_enabled,
                                sizeof(dst_enabled));
    if (ret < 0) {
        shell_error(sh, "Could not save the daylight-saving setting (error %d).",
                    ret);
        return ret;
    }

    k_mutex_lock(&time_zone_mutex, K_FOREVER);
    time_zone_config.dst_enabled = dst_enabled != 0U;
    k_mutex_unlock(&time_zone_mutex);

    shell_print(sh, "EU daylight saving set to %s.",
                dst_enabled != 0U ? "on" : "off");
    ret = timeprocess_refresh_display();
    if (ret < 0) {
        shell_error(sh, "DST setting saved, but display refresh failed (error %d).",
                    ret);
    }

    return ret;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
    time_zone_commands,
    SHELL_CMD(get, NULL, "Get the UTC offset", cmd_time_zone_get),
    SHELL_CMD_ARG(set, NULL, "Set a fixed UTC offset", cmd_time_zone_set,
                  1, 1),
    SHELL_CMD(list, NULL, "List available fixed UTC offsets",
              cmd_time_zone_list),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(
    time_dst_commands,
    SHELL_CMD(get, NULL, "Get daylight-saving status", cmd_time_dst_get),
    SHELL_CMD_ARG(set, NULL, "Enable or disable EU daylight saving",
                  cmd_time_dst_set, 1, 1),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(
    rtc_commands,
    SHELL_CMD(get, NULL, "Read date and time", cmd_rtc_get),
    SHELL_CMD(set, NULL, "Set date and time (YYYY-MM-DD HH:MM)", cmd_rtc_set),
    SHELL_CMD(zone, &time_zone_commands, "Time zone controls", NULL),
    SHELL_CMD(dst, &time_dst_commands, "Daylight-saving controls", NULL),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(time, &rtc_commands, "Date and time controls", NULL);

static int timeprocess_add_word_to_frame(
    enum word_id id, uint8_t frame[TIME_DISPLAY_HEIGHT][TIME_DISPLAY_WIDTH])
{
    const struct word_coord *coord = word_map_get(id);
    if (coord == NULL) {
        return -EINVAL;
    }

    for (uint8_t offset = 0; offset < coord->len; offset++) {
        uint8_t x = coord->x + offset;
        if (x >= TIME_DISPLAY_WIDTH || coord->y >= TIME_DISPLAY_HEIGHT) {
            return -EINVAL;
        }

        frame[coord->y][x] = 1U;
    }

    return 0;
}

static int timeprocess_build_time_frame(
    int hour, uint8_t slot,
    uint8_t frame[TIME_DISPLAY_HEIGHT][TIME_DISPLAY_WIDTH])
{
    memset(frame, 0, TIME_DISPLAY_HEIGHT * TIME_DISPLAY_WIDTH);

    int ret = timeprocess_add_word_to_frame(WORD_S, frame);
    if (ret == 0) {
        ret = timeprocess_add_word_to_frame(WORD_ISCH, frame);
    }

    const struct time_phrase *phrase = &time_phrases[slot];
    for (uint8_t i = 0; ret == 0 && i < phrase->count; i++) {
        ret = timeprocess_add_word_to_frame(phrase->words[i], frame);
    }

    uint8_t display_hour = (uint8_t)(hour % 12);
    if (slot >= 5) {
        display_hour = (uint8_t)((display_hour + 1U) % 12U);
    }

    if (ret == 0) {
        ret = timeprocess_add_word_to_frame(hour_words[display_hour], frame);
    }
    return ret;
}

static uint8_t timeprocess_sweep_level(
    uint8_t old_frame[TIME_DISPLAY_HEIGHT][TIME_DISPLAY_WIDTH],
    uint8_t new_frame[TIME_DISPLAY_HEIGHT][TIME_DISPLAY_WIDTH],
    uint8_t x, uint8_t y, int sweep_x)
{
    bool was_lit = old_frame[y][x] != 0U;
    bool should_be_lit = new_frame[y][x] != 0U;
    int distance = sweep_x - x;

    if (distance >= 0) {
        if (was_lit && should_be_lit) {
            return UINT8_MAX;
        }
        if (!should_be_lit) {
            return 0U;
        }

        if (distance == 0) {
            return TIME_SWEEP_FADE_IN_FIRST;
        }
        if (distance == 1) {
            return TIME_SWEEP_FADE_IN_SECOND;
        }
        if (distance == 2) {
            return TIME_SWEEP_FADE_IN_THIRD;
        }
        if (distance == 3) {
            return TIME_SWEEP_FADE_IN_FOURTH;
        }
        if (distance == 4) {
            return TIME_SWEEP_FADE_IN_FIFTH;
        }
        return UINT8_MAX;
    }

    int ahead = -distance;
    if (was_lit && should_be_lit) {
        return UINT8_MAX;
    }
    if (was_lit) {
        if (ahead == 1) {
            return TIME_SWEEP_FADE_OUT_FIRST;
        }
        if (ahead == 2) {
            return TIME_SWEEP_FADE_OUT_SECOND;
        }
        if (ahead == 3) {
            return TIME_SWEEP_FADE_OUT_THIRD;
        }
        if (ahead == 4) {
            return TIME_SWEEP_FADE_OUT_FOURTH;
        }
        if (ahead == 5) {
            return TIME_SWEEP_FADE_OUT_FIFTH;
        }
        return UINT8_MAX;
    }

    return 0U;
}

static int timeprocess_render_time(int hour, uint8_t slot, bool animate,
                                   int previous_hour, uint8_t previous_slot)
{
    uint8_t old_frame[TIME_DISPLAY_HEIGHT][TIME_DISPLAY_WIDTH];
    uint8_t new_frame[TIME_DISPLAY_HEIGHT][TIME_DISPLAY_WIDTH];
    int ret = timeprocess_build_time_frame(hour, slot, new_frame);
    if (ret != 0) {
        return ret;
    }

    if (animate) {
        ret = timeprocess_build_time_frame(previous_hour, previous_slot,
                                           old_frame);
        if (ret != 0) {
            return ret;
        }

        for (int sweep_step = 0;
             sweep_step < (TIME_DISPLAY_WIDTH + 3) * 2;
             sweep_step++) {
            int sweep_x = sweep_step / 2;
            for (uint8_t y = 0; y < TIME_DISPLAY_HEIGHT; y++) {
                for (uint8_t x = 0; x < TIME_DISPLAY_WIDTH; x++) {
                    uint8_t level =
                        timeprocess_sweep_level(old_frame, new_frame,
                                                x, y, sweep_x);
                    if ((sweep_step % 2) != 0) {
                        uint8_t next_level =
                            timeprocess_sweep_level(old_frame, new_frame,
                                                    x, y, sweep_x + 1);
                        level = (uint8_t)(((uint16_t)level + next_level) / 2U);
                    }
                    ret = display_set_pixel_level(x, y, level);
                    if (ret != 0) {
                        return ret;
                    }
                }
            }

            ret = display_request_update();
            if (ret < 0) {
                return ret;
            }
            k_msleep(TIME_SWEEP_STEP_MS);
        }
    } else {
        ret = display_clear_buffer();
        if (ret != 0) {
            return ret;
        }
        for (uint8_t y = 0; y < TIME_DISPLAY_HEIGHT; y++) {
            for (uint8_t x = 0; x < TIME_DISPLAY_WIDTH; x++) {
                ret = display_set_pixel_level(
                    x, y, new_frame[y][x] != 0U ? UINT8_MAX : 0U);
                if (ret != 0) {
                    return ret;
                }
            }
        }

        ret = display_request_update();
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
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
    return timeprocess_render_time(time.tm_hour, slot, false, 0, 0);
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
                    ret = timeprocess_render_time(time.tm_hour, slot,
                                                  last_hour >= 0, last_hour,
                                                  (uint8_t)last_slot);
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
