/* 2.8-inch 320x240 landscape test: RGB fills, border, bouncing rectangle.
 * Uses the ST7789 controller confirmed for the replacement module.
 * No joystick, game logic, Pico framebuffer, DMA, or second core is used.
 */
#include <stdio.h>

#include "pico/stdlib.h"
#include "config.h"
#include "st7789.h"

/* RGB565 packs red into 5 bits, green into 6 bits, and blue into 5 bits. */
#define COLOR_BLACK 0x0000
#define COLOR_RED 0xF800
#define COLOR_GREEN 0x07E0
#define COLOR_BLUE 0x001F
#define COLOR_YELLOW 0xFFE0
#define COLOR_WHITE 0xFFFF

/* A border checks all four edges of the configured pixel-address range. */
/**
 * Draw a white one-pixel border on black to check all four address limits.
 * @note Sends fills directly over blocking SPI and pauses for the configured
 *       colour hold time. No framebuffer is allocated.
 */
static void show_border(void) {
    st7789_fill_screen(COLOR_BLACK);
    st7789_fill_rect(0, 0, DISPLAY_WIDTH, 1, COLOR_WHITE);
    st7789_fill_rect(0, DISPLAY_HEIGHT - 1, DISPLAY_WIDTH, 1, COLOR_WHITE);
    st7789_fill_rect(0, 0, 1, DISPLAY_HEIGHT, COLOR_WHITE);
    st7789_fill_rect(DISPLAY_WIDTH - 1, 0, 1, DISPLAY_HEIGHT, COLOR_WHITE);
    sleep_ms(DEMO_COLOR_HOLD_MS);
}

/**
 * Run the configured number of direct-drawing rectangle animation steps.
 * @note Starts on black, erases the old rectangle, moves it by one pixel per axis,
 *       bounces at screen edges, then draws yellow at the new position. Separate
 *       erase/draw transfers and slow SPI can make the intermediate state visible.
 *       Pauses between updates and returns when this animation segment ends.
 */
static void animate_rectangle(void) {
    int x = 0;
    int y = 0;
    int dx = 1;  // One pixel per step makes changed edges easier to observe.
    int dy = 1;
    const int max_x = DISPLAY_WIDTH - DEMO_RECT_WIDTH;
    const int max_y = DISPLAY_HEIGHT - DEMO_RECT_HEIGHT;

    st7789_fill_screen(COLOR_BLACK);
    st7789_fill_rect(x, y, DEMO_RECT_WIDTH, DEMO_RECT_HEIGHT, COLOR_YELLOW);

    for (unsigned int frame = 0; frame < DEMO_ANIMATION_FRAMES; frame++) {
        sleep_ms(DEMO_FRAME_DELAY_MS);  // Transfer time also affects frame timing.
        st7789_fill_rect(x, y, DEMO_RECT_WIDTH, DEMO_RECT_HEIGHT, COLOR_BLACK);
        x += dx;
        y += dy;

        if (x <= 0 || x >= max_x) {
            dx = -dx;  // Reverse direction at a horizontal screen boundary.
            x = x <= 0 ? 0 : max_x;
        }
        if (y <= 0 || y >= max_y) {
            dy = -dy;
            y = y <= 0 ? 0 : max_y;
        }
        st7789_fill_rect(x, y, DEMO_RECT_WIDTH, DEMO_RECT_HEIGHT, COLOR_YELLOW);
    }
}

/**
 * Repeat colour fills, an edge border and the direct-drawing rectangle test.
 * @return Never returns during normal operation.
 * @note Initializes USB and the ST7789, prints each intended drawing operation,
 *       and sends it over blocking SPI. Serial text does not prove panel receipt.
 *       This exercise draws directly to the display rather than a RAM framebuffer.
 */
int main(void) {
    stdio_init_all();
    st7789_init();

    while (true) {
        // Repeat the description so a monitor opened after boot can see it.
        printf("2.8-inch TFT test: %ux%u, ST7789 driver\n",
               (unsigned int)DISPLAY_WIDTH, (unsigned int)DISPLAY_HEIGHT);
        printf("Display test: RED\n");
        st7789_fill_screen(COLOR_RED);
        sleep_ms(DEMO_COLOR_HOLD_MS);

        printf("Display test: GREEN\n");
        st7789_fill_screen(COLOR_GREEN);
        sleep_ms(DEMO_COLOR_HOLD_MS);

        printf("Display test: BLUE\n");
        st7789_fill_screen(COLOR_BLUE);
        sleep_ms(DEMO_COLOR_HOLD_MS);

        printf("Display test: WHITE border at all four edges\n");
        show_border();

        printf("Display test: moving YELLOW rectangle on BLACK\n");
        animate_rectangle();
    }
}
