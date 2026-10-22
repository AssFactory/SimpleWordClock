
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <errno.h>
#include <stdint.h>

#include "timeprocess/timeprocess.h"
#include "ambiente/ambiente.h"
#include "display/display.h"
#include <zephyr/shell/shell.h>
#include <pico/bootrom.h>


/* Thread intervals */
#define TIMEPROCESS_INTERVAL_MS  1000U
#define AMBIENTE_INTERVAL_MS     5000U

/* Thread stack sizes */
#define TIMEPROCESS_STACK_SIZE   1024
#define AMBIENTE_STACK_SIZE      1024

/* Thread priorities: lower number means higher priority */
#define TIMEPROCESS_PRIORITY     6
#define AMBIENTE_PRIORITY        6

K_THREAD_STACK_DEFINE(timeprocess_stack, TIMEPROCESS_STACK_SIZE);
K_THREAD_STACK_DEFINE(ambiente_stack, AMBIENTE_STACK_SIZE);

static struct k_thread timeprocess_thread_data;
static struct k_thread ambiente_thread_data;

/* Thread arguments must remain valid for the thread lifetime. */
static const uint32_t timeprocess_interval_ms =
    TIMEPROCESS_INTERVAL_MS;

static const uint32_t ambiente_interval_ms =
    AMBIENTE_INTERVAL_MS;

static int cmd_bootsel(const struct shell *sh, size_t argc, char **argv)
{
    shell_print(sh, "Rebooting into BOOTSEL/UF2 mode...");
    
    // Kurze Pause, damit die Shell die Nachricht noch senden kann
    k_msleep(100); 

    // Parameter 1: GPIO-Maske für LEDs, die im Bootsel-Modus blinken sollen (0 = keine)
    // Parameter 2: Interface-Auswahl (0 = USB Mass Storage / UF2)
    reset_usb_boot(0, 0); 
    
    return 0;
}

// Registriert den Top-Level-Befehl "bootsel" in der Zephyr-Shell
SHELL_CMD_REGISTER(bootsel, NULL, "Reboot into RP2040 BOOTSEL mode", cmd_bootsel);

int main(void)
{
    int ret = display_init();

    if (ret < 0) {
        printk("Display initialization failed: %d\n", ret);
        return ret;
    }

    k_tid_t tid = k_thread_create(
        &timeprocess_thread_data,
        timeprocess_stack,
        K_THREAD_STACK_SIZEOF(timeprocess_stack),
        timeprocess_thread,
        (void *)&timeprocess_interval_ms,
        NULL,
        NULL,
        TIMEPROCESS_PRIORITY,
        0,
        K_NO_WAIT
    );

    if (!tid) {
        printk("Failed to create timeprocess thread\n");
        return -EAGAIN;
    }

    k_thread_start(&timeprocess_thread_data);

    tid = k_thread_create(
        &ambiente_thread_data,
        ambiente_stack,
        K_THREAD_STACK_SIZEOF(ambiente_stack),
        ambiente_thread,
        (void *)&ambiente_interval_ms,
        NULL,
        NULL,
        AMBIENTE_PRIORITY,
        0,
        K_NO_WAIT
    );

    if (!tid) {
        printk("Failed to create ambiente thread\n");
        return -EAGAIN;
    }

    k_thread_start(&ambiente_thread_data);

    return 0;
}
