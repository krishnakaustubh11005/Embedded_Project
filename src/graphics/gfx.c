/* Original compact glyphs and drawing primitives layered over the framebuffer. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "graphics/framebuffer.h"
#include "graphics/gfx.h"

static const uint8_t letters[26][5] = {
 {2,5,7,5,5},{6,5,6,5,6},{3,4,4,4,3},{6,5,5,5,6},
 {7,4,6,4,7},{7,4,6,4,4},{3,4,5,5,3},{5,5,7,5,5},
 {7,2,2,2,7},{1,1,1,5,2},{5,5,6,5,5},{4,4,4,4,7},
 {5,7,7,5,5},{5,7,7,7,5},{2,5,5,5,2},{6,5,6,4,4},
 {2,5,5,3,1},{6,5,6,5,5},{3,4,2,1,6},{7,2,2,2,2},
 {5,5,5,5,7},{5,5,5,5,2},{5,5,7,7,5},{5,5,2,5,5},
 {5,5,2,2,2},{7,1,2,4,7}
};
static const uint8_t digits[10][5] = {
 {7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},
 {7,4,7,1,7},{7,4,7,5,7},{7,1,1,1,1},{7,5,7,5,7},{7,5,7,1,7}
};

/** Clear SRAM only; the board layer transmits it after composition. */
void gfx_clear(uint16_t color) { fb_clear(color); }

/** Fill the rectangle in RAM, with clipping delegated to fb_fill_rect(). */
void gfx_rect(int x, int y, int w, int h, uint16_t color) { fb_fill_rect(x,y,w,h,color); }

/** Outline a positive-size rectangle in RAM without changing its interior. */
void gfx_outline(int x, int y, int w, int h, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    gfx_rect(x,y,w,1,color); gfx_rect(x,y+h-1,w,1,color);
    gfx_rect(x,y,1,h,color); gfx_rect(x+w-1,y,1,h,color);
}

/** Rasterize a line with integer Bresenham steps; expected coordinates are screen-scale. */
void gfx_line(int x0, int y0, int x1, int y1, uint16_t color) {
    int dx=abs(x1-x0), sx=x0<x1?1:-1;
    int dy=-abs(y1-y0), sy=y0<y1?1:-1, error=dx+dy;
    while (true) {
        fb_set_pixel(x0,y0,color);
        if (x0==x1 && y0==y1) break;
        int twice=2*error;
        if (twice>=dy) { error+=dy; x0+=sx; }
        if (twice<=dx) { error+=dx; y0+=sy; }
    }
}

/** Rasterize a small circle/ring in RAM using squared distance and clipped pixels. */
void gfx_circle(int x, int y, int radius, uint16_t color, bool filled) {
    if (radius < 0 || radius > DISPLAY_WIDTH) return;
    int inner=(radius-1)*(radius-1);
    for (int row=-radius;row<=radius;row++) {
        for (int column=-radius;column<=radius;column++) {
            int d=column*column+row*row;
            if (d<=radius*radius && (filled || radius==0 || d>=inner)) {
                fb_set_pixel(x+column,y+row,color);
            }
        }
    }
}

/** Return one 3-bit glyph row; unsupported characters are rendered as a question mark. */
static uint8_t glyph_row(char c, int row) {
    if (c>='a' && c<='z') c=(char)(c-'a'+'A');
    if (c>='A' && c<='Z') return letters[c-'A'][row];
    if (c>='0' && c<='9') return digits[c-'0'][row];
    if (c==' ') return 0;
    if (c=='-') return row==2?7:0;
    if (c==':') return row==1 || row==3?2:0;
    if (c=='.') return row==4?2:0;
    if (c=='/') return (uint8_t)(1u << (row/2));
    if (c=='+') return row==2?7:(row==1||row==3?2:0);
    if (c=='>') return row==0||row==4?4:(row==2?1:2);
    if (c=='!') return row==3?0:2;
    if (c==',') return row==3?2:(row==4?4:0);
    if (c=='=') return row==1||row==3?7:0;
    if (c=='%') { static const uint8_t bits[]={5,1,2,4,5}; return bits[row]; }
    static const uint8_t question[]={7,1,2,0,2};
    return question[row];
}

/** Draw a valid null-terminated string into RAM; scale 1..6, no wrapping or SPI. */
void gfx_text(int x, int y, const char *text, int scale, uint16_t color) {
    if (scale<1 || scale>6) return;
    for (;*text && x<DISPLAY_WIDTH;text++,x+=4*scale) {
        for (int row=0;row<5;row++) {
            uint8_t bits=glyph_row(*text,row);
            for (int column=0;column<3;column++) {
                if (bits & (1u << (2-column))) gfx_rect(x+column*scale,y+row*scale,scale,scale,color);
            }
        }
    }
}

/** Format a number on the stack, then draw it using the shared glyph renderer. */
void gfx_number(int x, int y, unsigned int number, int scale, uint16_t color) {
    char text[16];
    snprintf(text,sizeof(text),"%u",number);
    gfx_text(x,y,text,scale,color);
}

/** Blend RGB565 colours channel by channel; amount 0..256 selects a..b.
 * @note Unpacks 5/6/5-bit channels, interpolates in integers, then repacks.
 */
uint16_t gfx_mix(uint16_t a, uint16_t b, unsigned int amount) {
    if (amount>256) amount=256;
    unsigned int keep=256-amount;
    unsigned int r=(((a>>11)&31u)*keep+((b>>11)&31u)*amount)>>8;
    unsigned int g=(((a>>5)&63u)*keep+((b>>5)&63u)*amount)>>8;
    unsigned int bl=((a&31u)*keep+(b&31u)*amount)>>8;
    return (uint16_t)((r<<11)|(g<<5)|bl);
}

/** Fill one row at a time, blending top..bottom; cost equals a normal fill. */
void gfx_gradient(int x, int y, int w, int h, uint16_t top, uint16_t bottom) {
    if (w<=0 || h<=0) return;
    for (int row=0;row<h;row++) {
        unsigned int amount=h>1?(unsigned int)(row*256/(h-1)):0;
        gfx_rect(x,y+row,w,1,gfx_mix(top,bottom,amount));
    }
}

/** Fill a rounded rectangle as horizontal spans.
 * @note Corner rows are inset by r - sqrt(r^2 - dy^2), the circle equation, so
 *       each row is one clipped rectangle fill. Radius is limited to half size.
 */
void gfx_round_rect(int x, int y, int w, int h, int radius, uint16_t color) {
    if (w<=0 || h<=0) return;
    if (radius>w/2) radius=w/2;
    if (radius>h/2) radius=h/2;
    if (radius<=0) { gfx_rect(x,y,w,h,color); return; }
    for (int row=0;row<h;row++) {
        float dy=0;
        if (row<radius) dy=radius-(row+0.5f);
        else if (row>=h-radius) dy=(row+0.5f)-(h-radius);
        int inset=0;
        if (dy>0) inset=(int)(radius-sqrtf(fmaxf(0,(float)(radius*radius)-dy*dy))+0.5f);
        gfx_rect(x+inset,y+row,w-2*inset,1,color);
    }
}

/** Fill every pixel within radius of segment (x0,y0)-(x1,y1).
 * @note Projects each pixel onto the segment (parameter t clamped 0..1) and
 *       compares squared distance, avoiding sqrt. Only the clipped bounding
 *       box is visited; each row is written as one span because the shape is convex.
 */
void gfx_capsule(float x0, float y0, float x1, float y1, float radius, uint16_t color) {
    if (!(radius>=0) || !isfinite(x0+y0+x1+y1)) return;
    int left=(int)floorf(fminf(x0,x1)-radius), right=(int)ceilf(fmaxf(x0,x1)+radius);
    int top=(int)floorf(fminf(y0,y1)-radius), bottom=(int)ceilf(fmaxf(y0,y1)+radius);
    if (left<0) left=0;
    if (top<0) top=0;
    if (right>DISPLAY_WIDTH-1) right=DISPLAY_WIDTH-1;
    if (bottom>DISPLAY_HEIGHT-1) bottom=DISPLAY_HEIGHT-1;
    float dx=x1-x0, dy=y1-y0, length2=dx*dx+dy*dy;
    float inverse=length2>0.0001f?1.0f/length2:0, r2=radius*radius;
    for (int row=top;row<=bottom;row++) {
        int first=-1, last=-1;
        for (int column=left;column<=right;column++) {
            float px=column-x0, py=row-y0;
            float t=fmaxf(0,fminf(1,(px*dx+py*dy)*inverse));
            float ex=px-t*dx, ey=py-t*dy;
            if (ex*ex+ey*ey<=r2) { if (first<0) first=column; last=column; }
            else if (first>=0) break;
        }
        if (first>=0) gfx_rect(first,row,last-first+1,1,color);
    }
}

/** Darken pixels already in RAM; used for pause and result overlays. */
void gfx_dim(int x, int y, int w, int h) { fb_darken_rect(x,y,w,h); }

/** Width of n glyphs is n*4*scale minus the final one-column gap. */
int gfx_text_width(const char *text, int scale) {
    int n=(int)strlen(text);
    return n?n*4*scale-scale:0;
}

/** Centre text on column cx using its exact rendered width. */
void gfx_text_center(int cx, int y, const char *text, int scale, uint16_t color) {
    gfx_text(cx-gfx_text_width(text,scale)/2,y,text,scale,color);
}

/** Draw the shadow first, offset by one font pixel, then the text on top. */
void gfx_text_shadow(int x, int y, const char *text, int scale, uint16_t color, uint16_t shadow) {
    int offset=scale>1?scale/2+1:1;
    gfx_text(x+offset,y+offset,text,scale,shadow);
    gfx_text(x,y,text,scale,color);
}

/** Centred variant of gfx_text_shadow(). */
void gfx_text_center_shadow(int cx, int y, const char *text, int scale, uint16_t color, uint16_t shadow) {
    gfx_text_shadow(cx-gfx_text_width(text,scale)/2,y,text,scale,color,shadow);
}
