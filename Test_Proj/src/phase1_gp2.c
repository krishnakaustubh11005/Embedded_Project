/* Phase 1 practice: poll the shield's active-low button A on Pico GP2. */
#include <stdio.h>

#include "pico/stdlib.h"
#include "config.h"

/**
 * Poll the configured active-low button and print its raw logic level.
 * @return Never returns during normal operation.
 * @note Enables an internal pull-up: released reads 1 and pressed reads 0.
 *       Sampling is periodic, without debounce; short presses may be missed.
 */
int main(void) {
    stdio_init_all();  // Initialize the USB serial output enabled in CMake.

    gpio_init(BUTTON_A_PIN);
    gpio_set_dir(BUTTON_A_PIN, GPIO_IN);  // Read the pin rather than drive it.
    gpio_pull_up(BUTTON_A_PIN);  // Hold the input high when the button is open.

    while (true) {
        int level = gpio_get(BUTTON_A_PIN);  // Sample the current logic level.
        printf("GP2: %d (0=pressed, 1=released)\n", level);

        // This is raw polling, not debouncing; short presses can be missed.
        sleep_ms(BUTTON_POLL_INTERVAL_MS);
    }
}
