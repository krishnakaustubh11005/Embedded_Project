/* Isolate columns beyond the observed square using the existing TFT driver. */
#include <stdio.h>

#include "pico/stdlib.h"
#include "config.h"
#include "st7789.h"

#define COLOR_BLACK 0x0000
#define COLOR_BLUE 0x001F
#define COLOR_YELLOW 0xFFE0

_Static_assert(STRIP_TEST_CURRENT_WIDTH < DISPLAY_WIDTH,
               "The observed width must leave a strip to test");

/**
 * Test the columns beyond the previously observed square image.
 * @return Never returns during normal operation.
 * @note Draws a blue reference area, then sends yellow and black to the remaining
 *       strip with configured holds. Uses blocking SPI; this visual diagnostic
 *       cannot distinguish bad addressing from faulty pixels without observation.
 */
int main(void) {
    stdio_init_all();
    st7789_init();
    const unsigned int remaining_width = DISPLAY_WIDTH - STRIP_TEST_CURRENT_WIDTH;

    while (true) {
        // Redraw the blue reference each cycle in case addressing wraps.
        st7789_fill_screen(COLOR_BLACK);
        st7789_fill_rect(0, 0, STRIP_TEST_CURRENT_WIDTH, DISPLAY_HEIGHT, COLOR_BLUE);
        printf("Strip test: blue reference, X=0..%u\n",
               (unsigned int)STRIP_TEST_CURRENT_WIDTH - 1);
        sleep_ms(STRIP_TEST_HOLD_MS);

        // Starting at X=240 with width 80 targets inclusive columns 240..319.
        printf("Strip test: YELLOW X=%u..%u, width=%u, height=%u\n",
               (unsigned int)STRIP_TEST_CURRENT_WIDTH,
               (unsigned int)DISPLAY_WIDTH - 1, remaining_width,
               (unsigned int)DISPLAY_HEIGHT);
        st7789_fill_rect(STRIP_TEST_CURRENT_WIDTH, 0, remaining_width,
                          DISPLAY_HEIGHT, COLOR_YELLOW);
        sleep_ms(STRIP_TEST_HOLD_MS);

        printf("Strip test: same strip BLACK\n");
        st7789_fill_rect(STRIP_TEST_CURRENT_WIDTH, 0, remaining_width,
                          DISPLAY_HEIGHT, COLOR_BLACK);
        sleep_ms(STRIP_TEST_HOLD_MS);
    }
}
