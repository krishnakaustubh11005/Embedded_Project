/* Drawing into one RGB565 image in RAM; these functions do not use SPI. */
#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>

#define FB_BLACK 0x0000
#define FB_WHITE 0xFFFF
#define FB_YELLOW 0xFFE0
#define FB_GREEN 0x07E0

/** Return the persistent read-only RGB565 framebuffer view; no copy is made. */
const uint16_t *fb_pixels(void);
/** Fill the whole RAM image with color; no SPI transfer occurs. */
void fb_clear(uint16_t color);
/** Set RAM pixel (x,y), zero-based; ignore off-screen coordinates. */
void fb_set_pixel(int x, int y, uint16_t color);
/** Fill a clipped RAM rectangle; nonpositive dimensions draw nothing. */
void fb_fill_rect(int x, int y, int width, int height, uint16_t color);
/* Eight rows of eight bits; a zero bit leaves the background unchanged. */
/** Draw eight row bytes as a clipped 8x8 sprite; zero bits preserve RAM pixels. */
void fb_draw_sprite8(int x, int y, const uint8_t rows[8], uint16_t color);
/** Draw digit 0..9 as a clipped 12x20 RAM pattern; other values draw nothing. */
void fb_draw_digit(int x, int y, unsigned int digit, uint16_t color);

#endif
