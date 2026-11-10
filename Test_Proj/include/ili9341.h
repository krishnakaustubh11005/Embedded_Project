/* Small blocking-SPI driver interface for the ILI9341 display exercise. */
#ifndef ILI9341_H
#define ILI9341_H

#include <stdint.h>

/** Initialize the historical ILI9341 module; the current ST7789 uses st7789_init(). */
void ili9341_init(void);
/** Send a solid clipped rectangle directly to an initialized ILI9341. */
void ili9341_fill_rect(unsigned int x, unsigned int y,
                       unsigned int width, unsigned int height, uint16_t color);
/** Fill the historical ILI9341 display using blocking SPI. */
void ili9341_fill_screen(uint16_t color);

#endif
