/* Small RAM-only graphics API; all colours are RGB565. */
#ifndef CONSOLE_GFX_H
#define CONSOLE_GFX_H
#include <stdbool.h>
#include <stdint.h>

/* Pack 8-bit red/green/blue into RGB565: 5 red, 6 green and 5 blue bits. */
#define RGB565(r,g,b) ((uint16_t)((((r)&0xF8u)<<8)|(((g)&0xFCu)<<3)|((b)>>3)))

/* Shared bright "party game" palette. P1 is always blue and P2 always red. */
enum {
    INK=0xFFFF, BLACK=0,
    BACKGROUND=RGB565(18,22,44), PANEL=RGB565(36,44,84), PANEL_LIGHT=RGB565(60,72,126),
    MUTED=RGB565(150,160,200), SHADOW=RGB565(8,10,22),
    ACCENT=RGB565(255,204,0), CYAN=RGB565(0,214,255), GREEN=RGB565(64,220,112),
    ORANGE=RGB565(255,144,32), PURPLE=RGB565(160,96,255), ROSE=RGB565(236,64,104),
    BLUE=RGB565(48,144,255), BLUE_DARK=RGB565(20,64,160),
    RED=RGB565(255,72,88), RED_DARK=RGB565(150,28,44),
    GRASS=RGB565(52,160,80), GRASS_DARK=RGB565(40,136,66)
};

/** Clear the RAM image with an RGB565 colour. */
void gfx_clear(uint16_t color);
/** Fill a clipped pixel rectangle; nonpositive dimensions draw nothing. */
void gfx_rect(int x, int y, int w, int h, uint16_t color);
/** Draw a one-pixel outline of a positive-size rectangle. */
void gfx_outline(int x, int y, int w, int h, uint16_t color);
/** Draw a clipped line between integer pixel coordinates. */
void gfx_line(int x0, int y0, int x1, int y1, uint16_t color);
/** Draw a filled circle or a one-pixel ring; radius must be nonnegative. */
void gfx_circle(int x, int y, int radius, uint16_t color, bool filled);
/** Draw uppercase 3x5 glyphs; scale is pixels per bit, advance is 4*scale. */
void gfx_text(int x, int y, const char *text, int scale, uint16_t color);
/** Format an unsigned integer using the same font, without allocating memory. */
void gfx_number(int x, int y, unsigned int number, int scale, uint16_t color);
/** Blend two RGB565 colours; amount 0 returns a, 256 returns b. */
uint16_t gfx_mix(uint16_t a, uint16_t b, unsigned int amount);
/** Fill a rectangle whose colour fades row by row from top to bottom. */
void gfx_gradient(int x, int y, int w, int h, uint16_t top, uint16_t bottom);
/** Fill a rectangle with circular corners of the given pixel radius. */
void gfx_round_rect(int x, int y, int w, int h, int radius, uint16_t color);
/** Fill a thick line with round ends ("capsule"); used for limbs and snakes. */
void gfx_capsule(float x0, float y0, float x1, float y1, float radius, uint16_t color);
/** Halve the brightness of an existing clipped area, e.g. behind a pop-up. */
void gfx_dim(int x, int y, int w, int h);
/** Return the pixel width of text drawn at scale (no trailing gap). */
int gfx_text_width(const char *text, int scale);
/** Draw text horizontally centred on column cx. */
void gfx_text_center(int cx, int y, const char *text, int scale, uint16_t color);
/** Draw text with a one-scale-unit drop shadow for readability on busy art. */
void gfx_text_shadow(int x, int y, const char *text, int scale, uint16_t color, uint16_t shadow);
/** Draw centred text with a drop shadow. */
void gfx_text_center_shadow(int cx, int y, const char *text, int scale, uint16_t color, uint16_t shadow);
#endif
