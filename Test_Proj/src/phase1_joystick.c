/* Phase 1 practice: print raw 12-bit readings from both joystick axes. */
#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "config.h"

/**
 * Sample both joystick axes through the ADC and print their raw readings.
 * @return Never returns during normal operation.
 * @note Enables analog operation on the configured GPIOs, selects X then Y ADC
 *       channels, and reads each sequentially. Values are 12-bit (0..4095), with
 *       no averaging, calibration or dead zone. Pauses between sample pairs.
 */
int main(void) {
    stdio_init_all();
    adc_init();  // Initialize the ADC hardware before taking measurements.

    // Select analog operation and disable digital input circuitry/pull-ups.
    adc_gpio_init(JOYSTICK_X_PIN);
    adc_gpio_init(JOYSTICK_Y_PIN);

    while (true) {
        adc_select_input(JOYSTICK_X_ADC_CHANNEL);
        unsigned int x = adc_read();  // Start a conversion and wait for its result.

        adc_select_input(JOYSTICK_Y_ADC_CHANNEL);
        unsigned int y = adc_read();  // Y is sampled after X, not simultaneously.

        printf("X=%u Y=%u\n", x, y);  // Raw readings range from 0 to 4095.
        sleep_ms(JOYSTICK_POLL_INTERVAL_MS);
    }
}
