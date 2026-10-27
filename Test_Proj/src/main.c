/* Blink the Pico 2 LED and print a USB counter once per configured blink. */
#include <stdio.h>

#include "pico/stdlib.h"
#include "config.h"

int main(void) {
    stdio_init_all();  // Initialize the USB serial output enabled in CMake.
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    unsigned int counter = 0;

    while (true) {
        gpio_put(LED_PIN, 1);
        sleep_ms(BLINK_HALF_PERIOD_MS);
        gpio_put(LED_PIN, 0);
        sleep_ms(BLINK_HALF_PERIOD_MS);

        printf("Counter: %u\n", counter);  // %u prints an unsigned integer.
        counter++;  // Increase the value for the next blink.
    }
}
