/* One SRAM framebuffer and small, clipped drawing functions for Phase 3. */
#include <stddef.h>

#include "config.h"
#include "graphics/framebuffer.h"

/* Static storage: 320*240*2 = 153600 bytes in SRAM, not on the call stack. */
static uint16_t framebuffer[DISPLAY_WIDTH * DISPLAY_HEIGHT];

/* Original 3x5 digit patterns. Each row uses the lowest three bits.
 * const allows the linker to keep this read-only table in flash.
 */
static const uint8_t digits[10][5] = {
    {7, 5, 5, 5, 7},  // 0: rows 111, 101, 101, 101, 111
    {2, 6, 2, 2, 7},  // 1
    {7, 1, 7, 4, 7},  // 2
    {7, 1, 7, 1, 7},  // 3
    {5, 5, 7, 1, 1},  // 4
    {7, 4, 7, 1, 7},  // 5
    {7, 4, 7, 5, 7},  // 6
    {7, 1, 1, 1, 1},  // 7
    {7, 5, 7, 5, 7},  // 8
    {7, 5, 7, 1, 7}   // 9
};

/**
 * Get a read-only view of the single framebuffer in SRAM.
 * @return Pointer to DISPLAY_WIDTH * DISPLAY_HEIGHT row-major RGB565 pixels.
 * @note No allocation or copy occurs. The pointer remains valid for the program
 *       lifetime, but drawing functions change its contents. Finish a transfer
 *       before drawing into the same buffer; this view is not a frozen snapshot.
 */
const uint16_t *fb_pixels(void) {
    return framebuffer;  // The display may read it but cannot modify via this pointer.
}

/**
 * Fill every pixel of the RAM framebuffer with one RGB565 colour.
 * @param color Packed 16-bit colour written to all array entries.
 * @note Changes SRAM only; call st7789_push_frame() to update the physical display.
 */
void fb_clear(uint16_t color) {
    for (size_t i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; i++) {
        framebuffer[i] = color;
    }
}

/**
 * Change one pixel in the RAM image using zero-based (column, row).
 * @param x Horizontal coordinate; zero is the first column.
 * @param y Vertical coordinate; zero is the first row.
 * @param color Packed RGB565 colour.
 * @note Out-of-screen coordinates are ignored. The array index is
 *       y * DISPLAY_WIDTH + x. No SPI transfer occurs.
 */
void fb_set_pixel(int x, int y, uint16_t color) {
    if (x < 0 || y < 0 || x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) {
        return;  // Prevent writes outside the framebuffer.
    }
    framebuffer[y * DISPLAY_WIDTH + x] = color;
}

/**
 * Fill a rectangle in SRAM, clipping it to the visible image bounds.
 * @param x Left column; may be negative for a partly off-screen rectangle.
 * @param y Top row; may be negative for a partly off-screen rectangle.
 * @param width Rectangle width in pixels; zero or negative values draw nothing.
 * @param height Rectangle height in pixels; zero or negative values draw nothing.
 * @param color Packed RGB565 fill colour.
 * @note Uses wider intermediate arithmetic for safe bounds calculation.
 *       Changes the framebuffer only and does not send it to the panel.
 */
void fb_fill_rect(int x, int y, int width, int height, uint16_t color) {
    if (width <= 0 || height <= 0) {
        return;
    }
    // Use wider arithmetic so clipping also handles extreme signed inputs.
    int64_t right = (int64_t)x + width;
    int64_t bottom = (int64_t)y + height;
    int left = x < 0 ? 0 : x;
    int top = y < 0 ? 0 : y;
    if (right > DISPLAY_WIDTH) {
        right = DISPLAY_WIDTH;
    }
    if (bottom > DISPLAY_HEIGHT) {
        bottom = DISPLAY_HEIGHT;
    }
    for (int row = top; row < bottom; row++) {
        for (int column = left; column < right; column++) {
            framebuffer[row * DISPLAY_WIDTH + column] = color;
        }
    }
}

/**
 * Darken a rectangle of the RAM image to half brightness.
 * @param x,y,width,height Area in pixels; clipped like fb_fill_rect().
 * @note Shifting each RGB565 value right by one halves red, green and blue at
 *       once; the mask 0x7BEF clears bits that would leak between channels.
 */
void fb_darken_rect(int x, int y, int width, int height) {
    if (width <= 0 || height <= 0) {
        return;
    }
    int64_t right = (int64_t)x + width;
    int64_t bottom = (int64_t)y + height;
    int left = x < 0 ? 0 : x;
    int top = y < 0 ? 0 : y;
    if (right > DISPLAY_WIDTH) {
        right = DISPLAY_WIDTH;
    }
    if (bottom > DISPLAY_HEIGHT) {
        bottom = DISPLAY_HEIGHT;
    }
    for (int row = top; row < bottom; row++) {
        for (int column = left; column < right; column++) {
            uint16_t *pixel = &framebuffer[row * DISPLAY_WIDTH + column];
            *pixel = (uint16_t)((*pixel >> 1) & 0x7BEFu);
        }
    }
}

/**
 * Draw an eight-by-eight one-bit sprite into the RAM image.
 * @param x Column where the sprite's left edge is placed.
 * @param y Row where the sprite's top edge is placed.
 * @param rows Valid array of eight row bytes; bit 7 is each row's leftmost pixel.
 * @param color RGB565 colour for set bits; zero bits preserve existing pixels.
 * @note Coordinates should be normal screen-scale values. Individual off-screen
 *       pixels are ignored by fb_set_pixel(). No SPI transfer occurs.
 */
void fb_draw_sprite8(int x, int y, const uint8_t rows[8], uint16_t color) {
    for (int row = 0; row < 8; row++) {
        for (int column = 0; column < 8; column++) {
            // Test from the most significant (leftmost) bit to the least.
            if (rows[row] & (0x80u >> column)) {
                fb_set_pixel(x + column, y + row, color);
            }
        }
    }
}

/**
 * Draw a decimal digit using the small font stored in flash.
 * @param x Left column of the rendered digit.
 * @param y Top row of the rendered digit.
 * @param digit Value 0..9; other values draw nothing.
 * @param color Packed RGB565 foreground colour.
 * @note Each 3x5 font bit becomes a 4x4 block, making a 12x20 pixel digit.
 *       Unset bits preserve the background. Drawing is clipped and affects RAM only.
 */
void fb_draw_digit(int x, int y, unsigned int digit, uint16_t color) {
    if (digit > 9) {
        return;
    }
    for (int row = 0; row < 5; row++) {
        for (int column = 0; column < 3; column++) {
            if (digits[digit][row] & (1u << (2 - column))) {
                // Enlarge each font pixel into a readable 4x4 block.
                fb_fill_rect(x + column * 4, y + row * 4, 4, 4, color);
            }
        }
    }
}
