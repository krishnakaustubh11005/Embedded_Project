/* Check display jumper wires with a meter; keep display signal ends unplugged. */
#include <stdio.h>

#include "pico/stdlib.h"
#include "config.h"

/* Test one output at a time so a DC meter can show each steady level. */
/**
 * Drive one diagnostic GPIO low, then high, holding each level for a meter.
 * @param pin Pico GPIO number of the jumper being checked.
 * @note Configures the pin as an output, blocks for DISPLAY_PIN_HOLD_MS at each
 *       level, prints the states, then leaves the output low. No SPI is generated.
 */
void test_pin(unsigned int pin) {
    gpio_init(pin);
    gpio_put(pin, 0);  // Set the output value before enabling the output driver.
    gpio_set_dir(pin, GPIO_OUT);

    printf("GP%u LOW: expect about 0 V for 5 seconds\n", pin);
    sleep_ms(DISPLAY_PIN_HOLD_MS);

    gpio_put(pin, 1);
    printf("GP%u HIGH: expect about 3.3 V for 5 seconds\n", pin);
    sleep_ms(DISPLAY_PIN_HOLD_MS);

    gpio_put(pin, 0);  // Leave this pin low while testing the next one.
}

/**
 * Check each configured display signal as a slow GPIO output in turn.
 * @return Never returns during normal operation.
 * @note Display signal ends should be unplugged for this wiring diagnostic.
 *       Calls test_pin() for CS, SCK, MOSI, DC and RESET; these levels are not SPI.
 */
int main(void) {
    stdio_init_all();

    while (true) {
        // These are slow GPIO checks, not a valid SPI command sequence.
        test_pin(DISPLAY_CS_PIN);
        test_pin(DISPLAY_SCK_PIN);
        test_pin(DISPLAY_MOSI_PIN);
        test_pin(DISPLAY_DC_PIN);
        test_pin(DISPLAY_RST_PIN);
    }
}
