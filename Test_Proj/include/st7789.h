/* Small blocking-SPI driver interface for the ST7789 display exercise. */
#ifndef ST7789_H
#define ST7789_H

#include <stdint.h>

/** Configure SPI/GPIO and reset/start the ST7789 using blocking delays. */
void st7789_init(void);
/** Send a solid clipped rectangle directly over blocking SPI; no framebuffer change. */
void st7789_fill_rect(unsigned int x, unsigned int y,
                       unsigned int width, unsigned int height, uint16_t color);
/** Send one solid colour to the full configured screen over blocking SPI. */
void st7789_fill_screen(uint16_t color);
/* pixels must contain DISPLAY_WIDTH * DISPLAY_HEIGHT RGB565 values. */
/** Send a stable full-size RGB565 array; blocks through final SPI bit, without panel acknowledgement. */
void st7789_push_frame(const uint16_t *pixels);

#endif
