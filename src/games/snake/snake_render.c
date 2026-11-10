/* Snake graphics in the RGB565 framebuffer: checkerboard field, rounded snake, apple. */
#include "config.h"
#include "graphics/gfx.h"
#include "games/snake/snake_game.h"

static const uint16_t FIELD_LIGHT=RGB565(132,200,92), FIELD_DARK=RGB565(118,186,80);
static const uint16_t SNAKE_HEAD=RGB565(70,150,255), SNAKE_TAIL=RGB565(24,64,180);

/** Top-left pixel of a grid cell; the playfield starts below the header. */
static int cell_x(unsigned int x) { return (int)x*SNAKE_CELL_SIZE; }
static int cell_y(unsigned int y) { return SNAKE_HEADER_HEIGHT+(int)y*SNAKE_CELL_SIZE; }

/** Draw the food as a small red apple with a highlight, stem and leaf. */
static void draw_food(SnakeCell food) {
    int x=cell_x(food.x)+SNAKE_CELL_SIZE/2, y=cell_y(food.y)+SNAKE_CELL_SIZE/2;
    gfx_circle(x,y+1,3,RED,true);
    gfx_rect(x-1,y-1,1,1,INK);
    gfx_rect(x,y-4,1,2,RGB565(110,60,20));
    gfx_rect(x+1,y-4,2,1,GRASS_DARK);
}

/** Draw both eyes on the head, looking along the movement direction. */
static void draw_eyes(int x, int y, SnakeDirection direction) {
    int ex[2]={2,5}, ey[2]={2,2};
    if (direction==SNAKE_DOWN) { ey[0]=ey[1]=5; }
    if (direction==SNAKE_RIGHT) { ex[0]=ex[1]=5; ey[1]=5; }
    if (direction==SNAKE_LEFT) { ex[0]=ex[1]=1; ey[1]=5; }
    for (unsigned int i=0;i<2;i++) {
        gfx_rect(x+ex[i],y+ey[i],2,2,INK);
        gfx_rect(x+ex[i]+(direction==SNAKE_RIGHT),y+ey[i]+(direction==SNAKE_DOWN),1,1,BLACK);
    }
}

/** Header: title, score pill, DEV badge, status pill and control hint. */
static void header(const SnakeGame *game) {
    gfx_gradient(0,0,DISPLAY_WIDTH,SNAKE_HEADER_HEIGHT,PANEL_LIGHT,BACKGROUND);
    gfx_text_shadow(8,6,"SNAKE",3,INK,SHADOW);
    gfx_round_rect(84,4,92,22,11,SHADOW);
    gfx_text(96,12,"SCORE",1,MUTED);
    gfx_number(122,8,game->score,2,ACCENT);
    if (game->developer) { gfx_round_rect(182,6,40,18,9,CYAN); gfx_text_center(202,10,"DEV",2,SHADOW); }
    const char *status="PLAY"; uint16_t color=GREEN;
    if (game->status==SNAKE_PAUSED) { status="PAUSED"; color=ACCENT; }
    if (game->status==SNAKE_DEAD) { status="GAME OVER"; color=RED; }
    if (game->status==SNAKE_WON) { status="YOU WIN"; color=ACCENT; }
    int w=gfx_text_width(status,1)+16;
    gfx_round_rect(DISPLAY_WIDTH-8-w,8,w,14,7,color);
    gfx_text(DISPLAY_WIDTH-w,12,status,1,SHADOW);
    gfx_text_center(160,30,"A UP  B RIGHT  C DOWN  D LEFT  K PAUSE",1,MUTED);
}

/**
 * Compose a complete Snake screen into the RAM framebuffer.
 * @param game Valid read-only game state with dimensions matching config.h.
 * @note Body colour fades from head to tail; neighbouring cells are bridged
 *       so the snake looks continuous. A cyan frame marks DEV wall wrapping.
 *       Does not send SPI data.
 */
void snake_draw(const SnakeGame *game) {
    header(game);
    for (unsigned int y=0;y<game->rows;y++) for (unsigned int x=0;x<game->columns;x++) {
        gfx_rect(cell_x(x),cell_y(y),SNAKE_CELL_SIZE,SNAKE_CELL_SIZE,((x+y)&1u)?FIELD_DARK:FIELD_LIGHT);
    }
    if (game->developer) gfx_outline(0,SNAKE_HEADER_HEIGHT,DISPLAY_WIDTH,DISPLAY_HEIGHT-SNAKE_HEADER_HEIGHT,CYAN);
    if (game->status!=SNAKE_WON) draw_food(game->food);
    for (unsigned int i=game->length;i-->1;) {
        SnakeCell c=game->body[i], n=game->body[i-1];
        uint16_t color=gfx_mix(SNAKE_HEAD,SNAKE_TAIL,i*256/game->length);
        gfx_round_rect(cell_x(c.x)+1,cell_y(c.y)+1,6,6,2,color);
        if (c.y==n.y && (c.x==n.x+1 || n.x==c.x+1)) {
            gfx_rect(cell_x(c.x<n.x?c.x:n.x)+4,cell_y(c.y)+1,8,6,color);
        } else if (c.x==n.x && (c.y==n.y+1 || n.y==c.y+1)) {
            gfx_rect(cell_x(c.x)+1,cell_y(c.y<n.y?c.y:n.y)+4,6,8,color);
        }
    }
    SnakeCell head=game->body[0];
    gfx_round_rect(cell_x(head.x),cell_y(head.y),8,8,3,game->status==SNAKE_DEAD?RED:SNAKE_HEAD);
    draw_eyes(cell_x(head.x),cell_y(head.y),game->direction);
}

/** Launcher picture: a short blue snake curling toward an apple. */
void snake_icon(int cx, int cy) {
    gfx_capsule(cx-16,cy+8,cx+6,cy+8,4,SNAKE_TAIL);
    gfx_capsule(cx+6,cy+8,cx+6,cy-4,4,gfx_mix(SNAKE_HEAD,SNAKE_TAIL,128));
    gfx_capsule(cx+6,cy-4,cx-8,cy-4,4,SNAKE_HEAD);
    gfx_circle(cx-10,cy-4,5,SNAKE_HEAD,true);
    gfx_rect(cx-13,cy-7,2,2,INK);
    gfx_circle(cx+16,cy-8,4,RED,true);
    gfx_rect(cx+16,cy-14,1,3,RGB565(110,60,20));
    gfx_rect(cx+17,cy-14,3,1,GRASS_DARK);
}
