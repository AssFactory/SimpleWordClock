
#include <zephyr/logging/log.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/shell/shell.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "display.h"
#include "../ambiente/ambiente.h"
#include "../timeprocess/timeprocess.h"
#include "../word_map/word_map.h"

LOG_MODULE_REGISTER(display, LOG_LEVEL_INF);

#define LED_STRIP_NODE DT_ALIAS(led_strip)
#define LED_COUNT DT_PROP(LED_STRIP_NODE, chain_length)

#define NO_LED UINT8_MAX

#define DISPLAY_WIDTH  12
#define DISPLAY_HEIGHT 10

#define SENSOR_MIN_BRIGHTNESS 0
#define SENSOR_MAX_BRIGHTNESS 1500
#define LED_STRIP_MIN_BRIGHTNESS 2
#define LED_STRIP_MAX_BRIGHTNESS 200

#define LED_STRIP_MAX_CURRENT 2500
#define LED_STRIP_IDLE_CURRENT 100
#define LED_MAX_CURRENT_PER_CHANNEL 20

#define DISPLAY_WORKQ_STACK_SIZE 2048
#define DISPLAY_WORKQ_PRIORITY   5

K_THREAD_STACK_DEFINE(display_workq_stack,
                      DISPLAY_WORKQ_STACK_SIZE);

static struct k_work_q display_workq;

K_SEM_DEFINE(led_test_done, 0, 1);

 K_MUTEX_DEFINE(pixel_buffer_lock);

struct display_brightness_limits {
    uint8_t min;
    uint8_t max;
};

static atomic_t brightness_limits =
    ATOMIC_INIT((LED_STRIP_MAX_BRIGHTNESS << 8) |
                LED_STRIP_MIN_BRIGHTNESS);

static void display_update_handler(struct k_work *work);
static void display_led_test_handler(struct k_work *work);

static int display_update(void);

K_WORK_DEFINE(display_update_work, display_update_handler);
K_WORK_DEFINE(display_led_test_work, display_led_test_handler);

/* Map logical grid coordinates to physical LED indices */
static const uint8_t led_map[DISPLAY_WIDTH * DISPLAY_HEIGHT] = {
    /* X:       0         1         2         3    4    5    6    7    8         9        10        11 */
    /* Y 0 */ NO_LED, NO_LED, NO_LED, 103, 102, 101, 100,  99,  98, NO_LED, NO_LED, NO_LED,
    /* Y 1 */ NO_LED,     88,     89,  90,  91,  92,  93,  94,  95,     96,     97, NO_LED,
    /* Y 2 */     87,     86,     85,  84,  83,  82,  81,  80,  79,     78,     77,     76,
    /* Y 3 */     64,     65,     66,  67,  68,  69,  70,  71,  72,     73,     74,     75,
    /* Y 4 */     63,     62,     61,  60,  59,  58,  57,  56,  55,     54,     53,     52,
    /* Y 5 */     40,     41,     42,  43,  44,  45,  46,  47,  48,     49,     50,     51,
    /* Y 6 */     39,     38,     37,  36,  35,  34,  33,  32,  31,     30,     29,     28,
    /* Y 7 */     16,     17,     18,  19,  20,  21,  22,  23,  24,     25,     26,     27,
    /* Y 8 */ NO_LED,     15,     14,  13,  12,  11,  10,   9,   8,      7,      6, NO_LED,
    /* Y 9 */ NO_LED, NO_LED, NO_LED,   0,   1,   2,   3,   4,   5, NO_LED, NO_LED, NO_LED
};

static const struct device *const led_strip = DEVICE_DT_GET(LED_STRIP_NODE);

/* Logical pixel buffer and physical output buffer */
static struct led_rgb pixel_buffer[LED_COUNT];
static enum display_color selected_color = DISPLAY_COLOR_WHITE;

static const struct led_rgb display_colors[DISPLAY_COLOR_COUNT] = {
    [DISPLAY_COLOR_WHITE] = { .r = 255, .g = 255, .b = 255 },
    [DISPLAY_COLOR_RED] = { .r = 255, .g = 0, .b = 0 },
    [DISPLAY_COLOR_BLUE] = { .r = 0, .g = 0, .b = 255 },
    [DISPLAY_COLOR_GREEN] = { .r = 0, .g = 255, .b = 0 },
    [DISPLAY_COLOR_CYAN] = { .r = 0, .g = 255, .b = 255 },
    [DISPLAY_COLOR_YELLOW] = { .r = 255, .g = 255, .b = 0 },
    [DISPLAY_COLOR_MAGENTA] = { .r = 255, .g = 0, .b = 255 },
};

static const char *const display_color_names[DISPLAY_COLOR_COUNT] = {
    [DISPLAY_COLOR_WHITE] = "white",
    [DISPLAY_COLOR_RED] = "red",
    [DISPLAY_COLOR_BLUE] = "blue",
    [DISPLAY_COLOR_GREEN] = "green",
    [DISPLAY_COLOR_CYAN] = "cyan",
    [DISPLAY_COLOR_YELLOW] = "yellow",
    [DISPLAY_COLOR_MAGENTA] = "magenta",
};

static int display_settings_set(const char *name, size_t len,
                               settings_read_cb read_cb, void *cb_arg)
{
    if (strcmp(name, "color") == 0) {
        uint8_t stored_color;

        if (len != sizeof(stored_color)) {
            return -EINVAL;
        }

        ssize_t bytes_read = read_cb(cb_arg, &stored_color,
                                     sizeof(stored_color));
        if (bytes_read != sizeof(stored_color)) {
            return bytes_read < 0 ? (int)bytes_read : -EIO;
        }

        if (stored_color >= DISPLAY_COLOR_COUNT) {
            return -EINVAL;
        }

        selected_color = (enum display_color)stored_color;
        return 0;
    }

    if (strcmp(name, "brightness") == 0) {
        struct display_brightness_limits stored_limits;

        if (len != sizeof(stored_limits)) {
            return -EINVAL;
        }

        ssize_t bytes_read = read_cb(cb_arg, &stored_limits,
                                     sizeof(stored_limits));
        if (bytes_read != sizeof(stored_limits)) {
            return bytes_read < 0 ? (int)bytes_read : -EIO;
        }

        if (stored_limits.min > stored_limits.max) {
            return -EINVAL;
        }

        atomic_set(&brightness_limits,
                   ((atomic_val_t)stored_limits.max << 8) |
                   stored_limits.min);
        return 0;
    }

    return -ENOENT;
}

SETTINGS_STATIC_HANDLER_DEFINE(display, "display", NULL, display_settings_set,
                               NULL, NULL);

int display_init(void)
{
    if (!device_is_ready(led_strip)) {
        LOG_ERR("LED strip is not ready.");
        return -ENODEV;
    }

    int ret = settings_subsys_init();
    if (ret != 0) {
        LOG_ERR("Could not initialize settings (error %d).", ret);
        return ret;
    }

    ret = settings_load();
    if (ret != 0) {
        LOG_ERR("Could not load display settings (error %d).", ret);
        return ret;
    }

    k_work_queue_start(&display_workq,
                       display_workq_stack,
                       K_THREAD_STACK_SIZEOF(display_workq_stack),
                       DISPLAY_WORKQ_PRIORITY,
                       NULL);

    LOG_INF("Display initialized.");
    return 0;
}

int display_clear_buffer(void)
{
    int ret = k_mutex_lock(&pixel_buffer_lock, K_FOREVER);
    if (ret != 0) {
        return ret; 
    }

    memset(pixel_buffer, 0, sizeof(pixel_buffer));

    k_mutex_unlock(&pixel_buffer_lock);

    return 0;
}

int display_clear(void)
{
    int ret = display_clear_buffer();
    if (ret != 0) {
        return ret;
    }

    return display_request_update();
}

static void display_update_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    int ret = display_update();

    if (ret < 0) {
        LOG_ERR("Could not update the display (error %d).", ret);
    }
}

/* Pending display updates can be coalesced by the work queue. */
int display_request_update(void)
{
    int ret = k_work_submit_to_queue(&display_workq,
                                     &display_update_work);

    if (ret < 0) {
        LOG_ERR("Could not submit a display update (error %d).", ret);
    }

    return ret;
}

int display_set_color(enum display_color color)
{
    if ((unsigned int)color >= DISPLAY_COLOR_COUNT) {
        return -EINVAL;
    }

    int ret = k_mutex_lock(&pixel_buffer_lock, K_FOREVER);
    if (ret != 0) {
        return ret;
    }

    if (selected_color == color) {
        k_mutex_unlock(&pixel_buffer_lock);
        return 0;
    }

    uint8_t stored_color = (uint8_t)color;
    ret = settings_save_one("display/color", &stored_color,
                            sizeof(stored_color));
    if (ret != 0) {
        k_mutex_unlock(&pixel_buffer_lock);
        LOG_ERR("Could not save the display color (error %d).", ret);
        return ret;
    }

    selected_color = color;
    for (size_t i = 0; i < LED_COUNT; i++) {
        if (pixel_buffer[i].r != 0U || pixel_buffer[i].g != 0U ||
            pixel_buffer[i].b != 0U) {
            pixel_buffer[i] = display_colors[selected_color];
        }
    }

    k_mutex_unlock(&pixel_buffer_lock);
    return 0;
}

int display_get_color(enum display_color *color)
{
    if (color == NULL) {
        return -EINVAL;
    }

    int ret = k_mutex_lock(&pixel_buffer_lock, K_FOREVER);
    if (ret != 0) {
        LOG_ERR("Could not lock the pixel buffer to read the display color "
                "(error %d).",
                ret);
        return ret;
    }

    *color = selected_color;
    k_mutex_unlock(&pixel_buffer_lock);
    return 0;
}

const char *display_color_name(enum display_color color)
{
    if ((unsigned int)color >= DISPLAY_COLOR_COUNT) {
        return "unknown";
    }

    return display_color_names[color];
}

uint8_t display_scale_brightness(uint16_t lux_val)
{
    atomic_val_t limits = atomic_get(&brightness_limits);
    uint8_t min_brightness = (uint8_t)(limits & 0xFF);
    uint8_t max_brightness = (uint8_t)((limits >> 8) & 0xFF);

    if (lux_val <= SENSOR_MIN_BRIGHTNESS) {
        return min_brightness;
    }
    if (lux_val >= SENSOR_MAX_BRIGHTNESS) {
        return max_brightness;
    }

    int32_t in_range = SENSOR_MAX_BRIGHTNESS - SENSOR_MIN_BRIGHTNESS;
    int32_t out_range = max_brightness - min_brightness;
    
    int32_t scaled = min_brightness + (((int32_t)(lux_val - SENSOR_MIN_BRIGHTNESS) * out_range) / in_range);

    return (uint8_t)scaled;
}

int display_get_brightness_limits(uint8_t *min, uint8_t *max)
{
    if (min == NULL || max == NULL) {
        return -EINVAL;
    }

    atomic_val_t limits = atomic_get(&brightness_limits);
    *min = (uint8_t)(limits & 0xFF);
    *max = (uint8_t)((limits >> 8) & 0xFF);
    return 0;
}

int display_set_brightness_limits(uint8_t min, uint8_t max)
{
    if (min > max) {
        return -EINVAL;
    }

    struct display_brightness_limits new_limits = {
        .min = min,
        .max = max,
    };

    int ret = settings_save_one("display/brightness", &new_limits,
                                sizeof(new_limits));
    if (ret != 0) {
        LOG_ERR("Could not save the display brightness limits (error %d).",
                ret);
        return ret;
    }

    atomic_set(&brightness_limits,
               ((atomic_val_t)max << 8) | min);
    return 0;
}

static int display_update(void)
{
    static struct led_rgb output_pixels[LED_COUNT];
    uint32_t total_pwm_ratio = 0;
    uint8_t brightness = display_scale_brightness(ambiente_get_lux());
    
    int ret = k_mutex_lock(&pixel_buffer_lock, K_FOREVER);
    if (ret != 0) {
        return ret; 
    }

    for (size_t i = 0; i < LED_COUNT; i++) {
        output_pixels[i].r = ((uint16_t)pixel_buffer[i].r * brightness) / 255U;
        output_pixels[i].g = ((uint16_t)pixel_buffer[i].g * brightness) / 255U;
        output_pixels[i].b = ((uint16_t)pixel_buffer[i].b * brightness) / 255U;

        total_pwm_ratio += output_pixels[i].r;
        total_pwm_ratio += output_pixels[i].g;
        total_pwm_ratio += output_pixels[i].b;
    }

    uint32_t estimated_led_current = (uint32_t)(LED_STRIP_IDLE_CURRENT + ((uint64_t)total_pwm_ratio * LED_MAX_CURRENT_PER_CHANNEL) / 255U);

    if (estimated_led_current > LED_STRIP_MAX_CURRENT) {
        LOG_WRN("Estimated LED current (%u mA) exceeds the %u mA limit; "
                "reducing brightness.",
                (unsigned int)estimated_led_current,
                (unsigned int)LED_STRIP_MAX_CURRENT);

        uint8_t safety_factor = (uint8_t)(((uint64_t)(LED_STRIP_MAX_CURRENT - LED_STRIP_IDLE_CURRENT) * 255U) /(estimated_led_current - LED_STRIP_IDLE_CURRENT));

        for (size_t i = 0; i < LED_COUNT; i++) {
            output_pixels[i].r = ((uint16_t)output_pixels[i].r * safety_factor) / 255U;
            output_pixels[i].g = ((uint16_t)output_pixels[i].g * safety_factor) / 255U;
            output_pixels[i].b = ((uint16_t)output_pixels[i].b * safety_factor) / 255U;
        }
    }

    k_mutex_unlock(&pixel_buffer_lock);

    return led_strip_update_rgb(led_strip, output_pixels, LED_COUNT);
}

/*
 * Set a pixel in the logical display buffer.
 * This function does not transmit data to the LED strip.
 */
int display_set_pixel(uint8_t x, uint8_t y, struct led_rgb color)
{
    if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) {
        LOG_ERR("Invalid pixel coordinates (x=%u, y=%u).",
                (unsigned int)x, (unsigned int)y);
        return -EINVAL;
    }

    uint8_t index = led_map[y * DISPLAY_WIDTH + x];

    if (index == NO_LED) {
        return 0;
    }

    if (index >= LED_COUNT) {
        LOG_ERR("Invalid LED mapping index (%u).", (unsigned int)index);
        return -EINVAL;
    }

    int ret = k_mutex_lock(&pixel_buffer_lock, K_FOREVER);
    
    if (ret != 0) {
        return ret;
    }

    if (color.r != 0U || color.g != 0U || color.b != 0U) {
        color = display_colors[selected_color];
    }

    pixel_buffer[index] = color;

    k_mutex_unlock(&pixel_buffer_lock);
    return 0;
}

int display_set_pixel_level(uint8_t x, uint8_t y, uint8_t level)
{
    if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) {
        LOG_ERR("Invalid pixel coordinates (x=%u, y=%u).",
                (unsigned int)x, (unsigned int)y);
        return -EINVAL;
    }

    uint8_t index = led_map[y * DISPLAY_WIDTH + x];
    if (index == NO_LED) {
        return 0;
    }
    if (index >= LED_COUNT) {
        LOG_ERR("Invalid LED mapping index (%u).", (unsigned int)index);
        return -EINVAL;
    }

    int ret = k_mutex_lock(&pixel_buffer_lock, K_FOREVER);
    if (ret != 0) {
        return ret;
    }

    const struct led_rgb color = display_colors[selected_color];
    pixel_buffer[index].r = ((uint16_t)color.r * level) / 255U;
    pixel_buffer[index].g = ((uint16_t)color.g * level) / 255U;
    pixel_buffer[index].b = ((uint16_t)color.b * level) / 255U;

    k_mutex_unlock(&pixel_buffer_lock);
    return 0;
}

/* Always signal completion so the waiting shell command can resume. */
static void display_led_test_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    const struct led_rgb white = {
        .r = 255,
        .g = 255,
        .b = 255
    };
    const struct led_rgb off = {
        .r = 0,
        .g = 0,
        .b = 0
    };

    bool test_failed = false;

    int ret = k_mutex_lock(&pixel_buffer_lock, K_FOREVER);
    
    if (ret != 0) {
        LOG_ERR("Could not lock the pixel buffer (error %d).", ret);
        k_sem_give(&led_test_done);
        return;
    }

    ret = display_clear_buffer();
    if (ret != 0) {
        LOG_ERR("Could not clear the pixel buffer (error %d).", ret);
        test_failed = true;
        goto finished;
    }

    for (uint8_t y = 0; y < DISPLAY_HEIGHT; y++) {
        for (uint8_t x = 0; x < DISPLAY_WIDTH; x++) {
            uint8_t index = led_map[y * DISPLAY_WIDTH + x];

            if (index == NO_LED) {
                continue;
            }

            ret = display_set_pixel(x, y, white);
            if (ret < 0) {
                LOG_ERR("Could not set pixel (%u, %u) (error %d).",
                        (unsigned int)x, (unsigned int)y, ret);
                test_failed = true;
                goto finished;
            }

            ret = display_update();
            if (ret < 0) {
                LOG_ERR("Could not update the display (error %d).", ret);
                test_failed = true;
                goto finished;
            }

            k_msleep(200);

            ret = display_set_pixel(x, y, off);
            if (ret < 0) {
                LOG_ERR("Could not clear pixel (%u, %u) (error %d).",
                        (unsigned int)x, (unsigned int)y, ret);
                test_failed = true;
                goto finished;
            }

            ret = display_update();
            if (ret < 0) {
                LOG_ERR("Could not update the display (error %d).", ret);
                test_failed = true;
                goto finished;
            }

            k_msleep(50);
        }
    }

finished:
    if (test_failed) {
        LOG_ERR("Pixel test failed.");
    } else {
            LOG_INF("Pixel test completed.");
    }

    k_mutex_unlock(&pixel_buffer_lock);

    k_sem_give(&led_test_done);
}

static int cmd_display_pixels(const struct shell *sh, size_t argc,
                              char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    shell_print(sh, "Starting pixel test.");

    /* Clear a possible leftover completion signal */
    k_sem_reset(&led_test_done);

    int ret = k_work_submit_to_queue(&display_workq,
                                 &display_led_test_work);

    if (ret < 0) {
        LOG_ERR("Could not submit the pixel test (error %d).", ret);
        return ret;
    }

    ret = k_sem_take(&led_test_done, K_FOREVER);

    if (ret != 0) {
        shell_error(sh, "Could not wait for the pixel test to finish "
                    "(error %d).", ret);
        return ret;
    }

    ret = timeprocess_refresh_display();
    if (ret < 0) {
        shell_error(sh, "Could not refresh the clock after the pixel test "
                    "(error %d).", ret);
        return ret;
    }

    shell_print(sh, "Pixel test completed.");
    return 0;
}

static int cmd_word_test(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    const struct led_rgb white = {
        .r = 255,
        .g = 255,
        .b = 255,
    };

    int ret = k_mutex_lock(&pixel_buffer_lock, K_FOREVER);
    if (ret != 0) {
        shell_error(sh, "Could not lock the pixel buffer (error %d).", ret);
        return ret;
    }

    shell_print(sh, "Starting word test; each word is shown for one second.");

    for (int id = 0; id < WORD_COUNT; id++) {
        const enum word_id word_id = (enum word_id)id;
        const struct word_coord *coord = word_map_get(word_id);
        const char *word_name = word_map_get_name(word_id);
        if (coord == NULL || word_name == NULL) {
            ret = -EINVAL;
            shell_error(sh, "No coordinates found for word ID %d.", id);
            break;
        }

        ret = display_clear_buffer();
        if (ret != 0) {
            shell_error(sh, "Could not clear the pixel buffer (error %d).",
                        ret);
            break;
        }

        for (uint8_t offset = 0; offset < coord->len; offset++) {
            ret = display_set_pixel(coord->x + offset, coord->y, white);
            if (ret != 0) {
                shell_error(sh, "Could not set %s at x=%u, y=%u (error %d).",
                            word_name,
                            (unsigned int)(coord->x + offset),
                            (unsigned int)coord->y,
                            ret);
                break;
            }
        }

        if (ret != 0) {
            break;
        }

        shell_print(sh, "Showing %s at x=%u, y=%u (length %u).",
                    word_name,
                    (unsigned int)coord->x,
                    (unsigned int)coord->y,
                    (unsigned int)coord->len);

        ret = display_update();
        if (ret < 0) {
            shell_error(sh, "Could not update the display for %s "
                        "(error %d).",
                        word_name, ret);
            break;
        }

        k_msleep(1000);
    }

    int clear_ret = display_clear_buffer();
    if (clear_ret == 0) {
        clear_ret = display_update();
    }
    if (ret == 0 && clear_ret < 0) {
        ret = clear_ret;
    }

    k_mutex_unlock(&pixel_buffer_lock);

    int refresh_ret = timeprocess_refresh_display();
    if (refresh_ret < 0) {
        shell_error(sh, "Could not refresh the clock after the word test "
                    "(error %d).",
                    refresh_ret);
        if (ret == 0) {
            ret = refresh_ret;
        }
    }

    if (ret == 0) {
        shell_print(sh, "Word test completed.");
    }

    return ret;
}

static int cmd_display_color_get(const struct shell *sh, size_t argc,
                                char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    enum display_color color;
    int ret = display_get_color(&color);
    if (ret != 0) {
        shell_error(sh, "Could not get the display color (error %d).", ret);
        return ret;
    }

    shell_print(sh, "Display color: %s.", display_color_name(color));
    return 0;
}

static int cmd_display_color_list(const struct shell *sh, size_t argc,
                                  char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    shell_print(sh, "Available colors:");
    for (enum display_color color = DISPLAY_COLOR_WHITE;
         color < DISPLAY_COLOR_COUNT; color++) {
        shell_print(sh, "%s", display_color_name(color));
    }

    return 0;
}

static int cmd_display_color_set(const struct shell *sh, size_t argc,
                                char **argv)
{
    if (argc != 2U) {
        shell_error(sh, "Usage: display color set <color>.");
        return -EINVAL;
    }

    enum display_color color;
    for (color = DISPLAY_COLOR_WHITE; color < DISPLAY_COLOR_COUNT; color++) {
        if (strcmp(argv[1], display_color_name(color)) == 0) {
            break;
        }
    }

    if (color == DISPLAY_COLOR_COUNT) {
        shell_error(sh, "Unknown color '%s'. Use 'display color list' "
                    "to list available colors.",
                    argv[1]);
        return -EINVAL;
    }

    int ret = display_set_color(color);
    if (ret != 0) {
        shell_error(sh, "Could not set the display color (error %d).", ret);
        return ret;
    }

    ret = display_request_update();
    if (ret < 0) {
        shell_error(sh, "Could not update the display (error %d).", ret);
        return ret;
    }

    shell_print(sh, "Display color set to %s.", display_color_name(color));
    return 0;
}

static int cmd_display_brightness_get(const struct shell *sh, size_t argc,
                                     char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    uint8_t min;
    uint8_t max;
    int ret = display_get_brightness_limits(&min, &max);
    if (ret != 0) {
        shell_error(sh, "Could not get the display brightness limits "
                    "(error %d).",
                    ret);
        return ret;
    }

    shell_print(sh, "Display brightness PWM limits: min=%u, max=%u.",
                (unsigned int)min, (unsigned int)max);
    return 0;
}

static int cmd_display_brightness_set(const struct shell *sh, size_t argc,
                                     char **argv)
{
    if (argc != 3U) {
        shell_error(sh, "Usage: display brightness set <min> <max>.");
        return -EINVAL;
    }

    uint8_t values[2];
    for (size_t i = 0; i < ARRAY_SIZE(values); i++) {
        char *end;
        if (argv[i + 1][0] == '-') {
            shell_error(sh, "Brightness values must be between 0 and 255.");
            return -EINVAL;
        }

        errno = 0;
        unsigned long value = strtoul(argv[i + 1], &end, 10);
        if (errno != 0 || end == argv[i + 1] || *end != '\0' ||
            value > UINT8_MAX) {
            shell_error(sh, "Invalid brightness value '%s'; use 0-255.",
                        argv[i + 1]);
            return -EINVAL;
        }
        values[i] = (uint8_t)value;
    }

    if (values[0] > values[1]) {
        shell_error(sh, "Minimum brightness must not exceed maximum.");
        return -EINVAL;
    }

    int ret = display_set_brightness_limits(values[0], values[1]);
    if (ret != 0) {
        shell_error(sh, "Could not save the display brightness limits "
                    "(error %d).",
                    ret);
        return ret;
    }

    ret = display_request_update();
    if (ret < 0) {
        shell_error(sh, "Could not update the display (error %d).", ret);
        return ret;
    }

    shell_print(sh, "Display brightness PWM limits set to min=%u, max=%u.",
                (unsigned int)values[0], (unsigned int)values[1]);
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
    display_color_commands,
    SHELL_CMD(get, NULL, "Get the display color", cmd_display_color_get),
    SHELL_CMD_ARG(set, NULL, "Set the display color", cmd_display_color_set,
                  1, 1),
    SHELL_CMD(list, NULL, "List available display colors",
              cmd_display_color_list),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(
    display_brightness_commands,
    SHELL_CMD(get, NULL, "Get brightness limits",
              cmd_display_brightness_get),
    SHELL_CMD_ARG(set, NULL, "Set minimum and maximum brightness (0-255)",
                  cmd_display_brightness_set, 2, 2),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(
    display_commands,
    SHELL_CMD(pixels, NULL, "Test each LED pixel individually",
              cmd_display_pixels),
    SHELL_CMD(words, NULL, "Display each word individually", cmd_word_test),
    SHELL_CMD(color, &display_color_commands, "Get or set display color", NULL),
    SHELL_CMD(brightness, &display_brightness_commands,
              "Get or set brightness limits", NULL),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(display, &display_commands, "Display controls", NULL);
