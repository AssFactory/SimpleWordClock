#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(wordclock, LOG_LEVEL_INF);

int main(void)
{
    LOG_INF("Hello World! Wordclock startet.");

    while (1) {
        k_sleep(K_SECONDS(1));
    }

    return 0;
}