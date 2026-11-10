/* Pong rendering: split-colour court like a two-player party game; no SPI or GPIO. */
#include "graphics/gfx.h"
#include "games/pong/pong.h"

/** Draw one rounded paddle with a lighter highlight stripe. */
static void paddle(Rect box, uint16_t color) {
    int x=(int)box.x, y=(int)box.y, w=(int)box.w, h=(int)box.h;
    gfx_round_rect(x-1,y-1,w+2,h+2,4,INK);
    gfx_round_rect(x,y,w,h,3,color);
    gfx_rect(x+2,y+3,2,h-6,gfx_mix(color,INK,140));
}

/** Header: title in the middle, a score pill for each player. */
static void header(const PongGame *game) {
    gfx_gradient(0,0,320,34,PANEL_LIGHT,BACKGROUND);
    gfx_text_center_shadow(160,9,"PONG",3,INK,SHADOW);
    gfx_round_rect(8,6,96,24,12,BLUE);
    gfx_text(18,13,"P1",2,INK); gfx_number(70,9,game->score[0],3,INK);
    gfx_round_rect(216,6,96,24,12,RED);
    gfx_text(226,13,"P2",2,INK); gfx_number(278,9,game->score[1],3,INK);
}

/** Draw match scores, tinted halves, paddles and the ball with a short trail.
 * @note The trail is three dimmer circles placed back along the velocity; it
 *       reads the state only and changes RAM pixels only.
 */
void pong_draw(const void *state) {
    const PongGame *game=state;
    header(game);
    gfx_rect(0,34,320,206,BACKGROUND);
    gfx_gradient(8,36,152,184,RGB565(28,64,140),RGB565(16,36,88));
    gfx_gradient(160,36,152,184,RGB565(140,30,52),RGB565(88,18,36));
    gfx_number(64,104,game->score[0],6,RGB565(48,90,170));
    gfx_number(232,104,game->score[1],6,RGB565(170,52,72));
    gfx_outline(8,36,304,184,INK);
    for (int y=40;y<216;y+=12) gfx_rect(159,y,2,6,INK);
    gfx_circle(160,128,22,gfx_mix(INK,BACKGROUND,140),false);
    paddle(game->paddle[0],BLUE);
    paddle(game->paddle[1],RED);
    Vec2 p=game->ball.position, v=game->ball.velocity;
    for (int i=3;i>=1;i--) {
        gfx_circle((int)(p.x-v.x*0.012f*i),(int)(p.y-v.y*0.012f*i),4-i/2,gfx_mix(ACCENT,BACKGROUND,60*i),true);
    }
    gfx_circle((int)p.x,(int)p.y,5,ACCENT,true);
    gfx_circle((int)p.x,(int)p.y,3,INK,true);
    if (game->serve_seconds>0 && !game->over) {
        gfx_round_rect(104,90,112,26,13,SHADOW);
        gfx_text_center(160,98,"GET READY",2,ACCENT);
    }
    gfx_text_center(160,228,"STICK MOVES YOUR PADDLE   K PAUSE",1,MUTED);
}

/** Launcher picture: two paddles and a ball with a trail. */
void pong_icon(int cx, int cy) {
    gfx_round_rect(cx-20,cy-12,5,18,2,BLUE);
    gfx_round_rect(cx+15,cy-4,5,18,2,RED);
    gfx_circle(cx-6,cy+4,2,gfx_mix(ACCENT,INK,128),true);
    gfx_circle(cx,cy+1,3,ACCENT,true);
    gfx_circle(cx+6,cy-2,4,INK,true);
}
