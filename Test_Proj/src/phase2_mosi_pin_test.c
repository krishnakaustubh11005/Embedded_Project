/* Toggle GP19 slowly so a DC multimeter can distinguish its high and low levels. */
#include <stdio.h>

#include "pico/stdlib.h"
#include "config.h"

/**
 * Alternate the MOSI GPIO between high and low once per second.
 * @return Never returns during normal operation.
 * @note Drives ordinary GPIO levels, not SPI data, and prints each state over USB.
 *       Used with the display jumper disconnected to check the Pico output.
 */
int main(void) {
    stdio_init_all();

    gpio_init(DISPLAY_MOSI_PIN);
    gpio_put(DISPLAY_MOSI_PIN, 0);  // Set a known level before enabling output.
    gpio_set_dir(DISPLAY_MOSI_PIN, GPIO_OUT);

    while (true) {
        gpio_put(DISPLAY_MOSI_PIN, 1);
        printf("GP%u HIGH: expect about 3.3 V\n", DISPLAY_MOSI_PIN);
        sleep_ms(1000);

        gpio_put(DISPLAY_MOSI_PIN, 0);
        printf("GP%u LOW: expect about 0 V\n", DISPLAY_MOSI_PIN);
        sleep_ms(1000);
    }
}
