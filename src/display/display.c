
#include <zephyr/logging/log.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/util.h>

#include <errno.h>
#include <stdint.h>
#include <stddef.h>
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

#define SENSOR_MIN_BRIGHTNESS 100
#define SENSOR_MAX_BRIGHTNESS 3000
#define LED_STRIP_MIN_BRIGHTNESS 150
#define LED_STRIP_MAX_BRIGHTNESS 200

#define LED_STRIP_MAX_CURRENT 2500
#define LED_STRIP_IDLE_CURRENT 100
#define LED_MAX_CURRENT_PER_CHANNEL 20

/* Dedicated display work queue */
#define DISPLAY_WORKQ_STACK_SIZE 2048
#define DISPLAY_WORKQ_PRIORITY   5

K_THREAD_STACK_DEFINE(display_workq_stack,
                      DISPLAY_WORKQ_STACK_SIZE);

static struct k_work_q display_workq;

/* Signal that the LED test has finished */
K_SEM_DEFINE(led_test_done, 0, 1);

/* Prevent multiple access to pixel buffer */
 K_MUTEX_DEFINE(pixel_buffer_lock);

 /* Work handlers */
static void display_update_handler(struct k_work *work);
static void display_led_test_handler(struct k_work *work);

/* Internal function declaration */
static int display_update(void);

/* Work items */
K_WORK_DEFINE(display_update_work, display_update_handler);
K_WORK_DEFINE(display_led_test_work, display_led_test_handler);

/*
 * Gamma correction lookup table.
 * Reference:
 * https://learn.adafruit.com/led-tricks-gamma-correction/the-quick-fix
 */
static const uint8_t gamma_table[256] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255
};

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

/*
 * Initialize the display and start its dedicated work queue.
 */
int display_init(void)
{
    if (!device_is_ready(led_strip)) {
        LOG_ERR("LED strip is not ready");
        return -ENODEV;
    }

    k_work_queue_start(&display_workq,
                       display_workq_stack,
                       K_THREAD_STACK_SIZEOF(display_workq_stack),
                       DISPLAY_WORKQ_PRIORITY,
                       NULL);

    LOG_INF("Display initialized");
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

/*
 * Execute a normal display update from the display work queue.
 */
static void display_update_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    int ret = display_update();

    if (ret < 0) {
        LOG_ERR("Display update failed: %d", ret);
    }
}

/*
 * Request a normal display update.
 * Multiple requests can be coalesced by the work queue.
 */
int display_request_update(void)
{
    int ret = k_work_submit_to_queue(&display_workq,
                                     &display_update_work);

    if (ret < 0) {
        LOG_ERR("Failed to submit display update: %d", ret);
    }

    return ret;
}

uint8_t display_scale_brightness(uint16_t lux_val)
{
    if (lux_val <= SENSOR_MIN_BRIGHTNESS) {
        return LED_STRIP_MIN_BRIGHTNESS;
    }
    if (lux_val >= SENSOR_MAX_BRIGHTNESS) {
        return LED_STRIP_MAX_BRIGHTNESS;
    }

    int32_t in_range = SENSOR_MAX_BRIGHTNESS - SENSOR_MIN_BRIGHTNESS;
    int32_t out_range = LED_STRIP_MAX_BRIGHTNESS - LED_STRIP_MIN_BRIGHTNESS;
    
    int32_t scaled = LED_STRIP_MIN_BRIGHTNESS + (((int32_t)(lux_val - SENSOR_MIN_BRIGHTNESS) * out_range) / in_range);

    return (uint8_t)scaled;
}

/*
 * Apply brightness, gamma correction and software current limiting,
 * then transmit the resulting RGB values to the LED strip.
 */
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
        uint8_t r = ((uint16_t)pixel_buffer[i].r * brightness) / 255U;
        uint8_t g = ((uint16_t)pixel_buffer[i].g * brightness) / 255U;
        uint8_t b = ((uint16_t)pixel_buffer[i].b * brightness) / 255U;

        output_pixels[i].r = gamma_table[r];
        output_pixels[i].g = gamma_table[g];
        output_pixels[i].b = gamma_table[b];

        if (output_pixels[i].r != 0U ||
            output_pixels[i].g != 0U ||
            output_pixels[i].b != 0U) {
            LOG_INF("Pixel %zu: Input RGB(%u, %u, %u) -> Brightness-scaled RGB(%u, %u, %u) -> Gamma-corrected RGB(%u, %u, %u)",
                    i,
                    (unsigned int)pixel_buffer[i].r,
                    (unsigned int)pixel_buffer[i].g,
                    (unsigned int)pixel_buffer[i].b,
                    (unsigned int)r,
                    (unsigned int)g,
                    (unsigned int)b,
                    (unsigned int)output_pixels[i].r,
                    (unsigned int)output_pixels[i].g,
                    (unsigned int)output_pixels[i].b);
        }

        total_pwm_ratio += output_pixels[i].r;
        total_pwm_ratio += output_pixels[i].g;
        total_pwm_ratio += output_pixels[i].b;
    }

    uint32_t estimated_led_current = (uint32_t)(LED_STRIP_IDLE_CURRENT + ((uint64_t)total_pwm_ratio * LED_MAX_CURRENT_PER_CHANNEL) / 255U);

    if (estimated_led_current > LED_STRIP_MAX_CURRENT) {
        LOG_WRN("Estimated LED current %u mA exceeds budget %u mA; dimming LEDs", (unsigned int)estimated_led_current, (unsigned int)LED_STRIP_MAX_CURRENT);

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
        LOG_ERR("Invalid pixel coordinates: x=%u y=%u",
                (unsigned int)x, (unsigned int)y);
        return -EINVAL;
    }

    uint8_t index = led_map[y * DISPLAY_WIDTH + x];

    /* The logical position has no physical LED */
    if (index == NO_LED) {
        return 0;
    }

    if (index >= LED_COUNT) {
        LOG_ERR("Invalid LED mapping index: %u", (unsigned int)index);
        return -EINVAL;
    }

    int ret = k_mutex_lock(&pixel_buffer_lock, K_FOREVER);
    
    if (ret != 0) {
        return ret;
    }

    pixel_buffer[index] = color;

    k_mutex_unlock(&pixel_buffer_lock);
    return 0;
}

/*
 * Run the complete LED test inside the display work queue.
 * Always signal completion, including when an error occurs.
 */
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
        LOG_ERR("Failed to lock pixel buffer: %d", ret);
        k_sem_give(&led_test_done);
        return;
    }

    ret = display_clear_buffer();
    if (ret != 0) {
        LOG_ERR("Failed to clear pixel buffer: %d", ret);
        test_failed = true;
        goto finished;
    }

    for (uint8_t y = 0; y < DISPLAY_HEIGHT; y++) {
        for (uint8_t x = 0; x < DISPLAY_WIDTH; x++) {
            uint8_t index = led_map[y * DISPLAY_WIDTH + x];

            /* Skip logical positions without a physical LED */
            if (index == NO_LED) {
                continue;
            }

            ret = display_set_pixel(x, y, white);
            if (ret < 0) {
                LOG_ERR("Failed to set pixel (%u, %u): %d",
                        (unsigned int)x, (unsigned int)y, ret);
                test_failed = true;
                goto finished;
            }

            ret = display_update();
            if (ret < 0) {
                LOG_ERR("Failed to update display: %d", ret);
                test_failed = true;
                goto finished;
            }

            k_msleep(200);

            ret = display_set_pixel(x, y, off);
            if (ret < 0) {
                LOG_ERR("Failed to clear pixel (%u, %u): %d",
                        (unsigned int)x, (unsigned int)y, ret);
                test_failed = true;
                goto finished;
            }

            ret = display_update();
            if (ret < 0) {
                LOG_ERR("Failed to update display: %d", ret);
                test_failed = true;
                goto finished;
            }

            k_msleep(50);
        }
    }

finished:
    if (test_failed) {
        LOG_ERR("LED test failed");
    } else {
        LOG_INF("LED test completed");
    }

    k_mutex_unlock(&pixel_buffer_lock);

    /* Wake the shell command waiting for the test */
    k_sem_give(&led_test_done);
}

/*
 * Shell command: led_test
 */
static int cmd_led_test(const struct shell *sh, size_t argc, char **argv){
    
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    shell_print(sh, "Starting LED test...");

    /* Clear a possible leftover completion signal */
    k_sem_reset(&led_test_done);

    int ret = k_work_submit_to_queue(&display_workq,
                                 &display_led_test_work);

    if (ret < 0) {
        LOG_ERR("Failed to submit LED test: %d", ret);
        return ret;
    }

    /* Wait until the work handler signals completion */
    ret = k_sem_take(&led_test_done, K_FOREVER);

    if (ret != 0) {
    shell_error(sh, "Failed waiting for LED test: %d", ret);
    return ret;
}

ret = timeprocess_refresh_display();
if (ret < 0) {
    shell_error(sh, "Uhrzeit konnte nach dem LED-Test nicht angezeigt werden: %d",
                ret);
    return ret;
}

shell_print(sh, "LED test finished");
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
        shell_error(sh, "Pixelpuffer konnte nicht gesperrt werden: %d", ret);
        return ret;
    }

    shell_print(sh, "Worttest: jedes Wort wird 1 Sekunde angezeigt.");

    for (int id = 0; id < WORD_COUNT; id++) {
        const enum word_id word_id = (enum word_id)id;
        const struct word_coord *coord = word_map_get(word_id);
        const char *word_name = word_map_get_name(word_id);
        if (coord == NULL || word_name == NULL) {
            ret = -EINVAL;
            shell_error(sh, "Keine Koordinaten fuer Wort-ID %d", id);
            break;
        }

        ret = display_clear_buffer();
        if (ret != 0) {
            shell_error(sh, "Pixelpuffer konnte nicht geleert werden: %d", ret);
            break;
        }

        for (uint8_t offset = 0; offset < coord->len; offset++) {
            ret = display_set_pixel(coord->x + offset, coord->y, white);
            if (ret != 0) {
                shell_error(sh, "Fehler bei %s an x=%u y=%u: %d",
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

        shell_print(sh, "%s: Start x=%u y=%u, Laenge=%u",
                    word_name,
                    (unsigned int)coord->x,
                    (unsigned int)coord->y,
                    (unsigned int)coord->len);

        ret = display_update();
        if (ret < 0) {
            shell_error(sh, "Display-Update fuer %s fehlgeschlagen: %d",
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
        shell_error(sh, "Uhrzeit konnte nach dem Worttest nicht angezeigt werden: %d",
                    refresh_ret);
        if (ret == 0) {
            ret = refresh_ret;
        }
    }

    if (ret == 0) {
        shell_print(sh, "Worttest abgeschlossen.");
    }

    return ret;
}


SHELL_STATIC_SUBCMD_SET_CREATE(
    display_commands,
    SHELL_CMD(test, NULL, "Get actual brightness", cmd_led_test),
    SHELL_CMD(words, NULL, "Alle Woerter einzeln anzeigen", cmd_word_test),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(display, &display_commands, "Ambiente functions", NULL);
