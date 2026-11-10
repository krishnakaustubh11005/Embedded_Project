/* Phase 1 practice: print all six shield buttons and the joystick click. */
#include <stdio.h>

#include "pico/stdlib.h"
#include "config.h"

/* A function parameter lets the same setup operate on different GPIO pins. */
/**
 * Configure one active-low button as a digital input with a pull-up.
 * @param pin Pico GPIO number connected to the button.
 * @note Released reads high; pressing a button wired to ground reads low.
 *       This configures the input but performs no sampling or debouncing.
 */
void setup_button(unsigned int pin) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);  // Active-low buttons read high when released.
}

/**
 * Initialize configured buttons and repeatedly print their raw GPIO samples.
 * @return Never returns during normal operation.
 * @note Uses pull-ups and periodic polling, without debounce or press counting.
 *       Disabled inputs are not sampled; use config.h to identify active GPIOs.
 */
int main(void) {
    stdio_init_all();

    setup_button(BUTTON_A_PIN);
    setup_button(BUTTON_B_PIN);
    setup_button(BUTTON_C_PIN);
    setup_button(BUTTON_D_PIN);
    /*setup_button(BUTTON_E_PIN);
    setup_button(BUTTON_F_PIN); */
    setup_button(JOYSTICK_BUTTON_PIN);

    while (true) {
        // Each %d prints the matching gpio_get result: 0 pressed, 1 released.
        printf("A=%d B=%d C=%d D=%d E=%d F=%d K=%d\n",
               gpio_get(BUTTON_A_PIN),
               gpio_get(BUTTON_B_PIN),
               gpio_get(BUTTON_C_PIN),
               gpio_get(BUTTON_D_PIN),
               /*gpio_get(BUTTON_E_PIN),
               gpio_get(BUTTON_F_PIN),*/
               gpio_get(JOYSTICK_BUTTON_PIN));

        // Polling shows raw levels; it does not debounce or count presses.
        sleep_ms(BUTTON_POLL_INTERVAL_MS);
    }
}
