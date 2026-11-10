/* Soccer Pool rendering: striped pitch, netted goals, shiny disks, aim arrow. */
#include <math.h>
#include "graphics/gfx.h"
#include "games/soccer_pool/soccer.h"

/** Draw a shaded disk: drop shadow, rim, face colour and a highlight. */
static void disk(int x, int y, uint16_t color, unsigned int number) {
    gfx_circle(x+1,y+2,9,RGB565(20,70,36),true);
    gfx_circle(x,y,9,gfx_mix(color,BLACK,90),true);
    gfx_circle(x,y,7,color,true);
    gfx_circle(x-5,y-4,1,gfx_mix(color,INK,150),true);
    gfx_number(x-1,y-2,number,1,INK);
}

/** Draw a goal mouth with a grid net behind the line. */
static void goal(int x) {
    gfx_rect(x,104,8,48,RGB565(220,230,240));
    for (int y=106;y<152;y+=4) gfx_rect(x,y,8,1,RGB565(150,160,175));
    for (int i=1;i<8;i+=3) gfx_rect(x+i,104,1,48,RGB565(150,160,175));
}

/** Header: score pills and a banner naming whose turn it is, plus power cells. */
static void header(const SoccerGame *game) {
    gfx_gradient(0,0,320,34,PANEL_LIGHT,BACKGROUND);
    gfx_round_rect(6,5,64,24,12,BLUE);
    gfx_text(14,12,"P1",2,INK); gfx_number(46,8,game->score[0],3,INK);
    gfx_round_rect(250,5,64,24,12,RED);
    gfx_text(258,12,"P2",2,INK); gfx_number(290,8,game->score[1],3,INK);
    uint16_t turn=game->turn==0?BLUE:RED;
    gfx_round_rect(78,5,164,24,12,SHADOW);
    gfx_text_center(130,9,game->moving?"ROLLING":(game->turn==0?"P1 TURN":"P2 TURN"),2,game->moving?MUTED:turn);
    gfx_text(186,8,"POWER",1,MUTED);
    for (unsigned int bar=0;bar<4;bar++) {
        gfx_round_rect(186+(int)bar*12,17,10,7,3,bar<=game->power?ACCENT:PANEL_LIGHT);
    }
}

/** Draw read-only match state with team colours, active player and selected disk. */
void soccer_draw(const void *state) {
    const SoccerGame *game=state;
    header(game);
    gfx_rect(0,34,320,206,BACKGROUND);
    for (int x=8;x<312;x+=19) gfx_rect(x,36,x+19>312?312-x:19,184,((x-8)/19)&1?GRASS_DARK:GRASS);
    uint16_t line=RGB565(220,245,220);
    gfx_outline(8,36,304,184,line);
    gfx_line(160,36,160,219,line); gfx_circle(160,128,25,line,false); gfx_circle(160,128,2,line,true);
    gfx_outline(8,84,32,88,line); gfx_outline(280,84,32,88,line);
    goal(0); goal(312);
    for (unsigned int i=0;i<10;i++) {
        int x=(int)game->bodies[i].position.x, y=(int)game->bodies[i].position.y;
        bool selected=!game->moving && !game->over && i==game->turn*5+game->selected;
        if (selected) { gfx_circle(x,y,13,ACCENT,true); gfx_circle(x,y,11,SHADOW,true); }
        disk(x,y,i<5?BLUE:RED,i%5+1);
        if (selected) {
            float length=20+(game->power+1)*9;
            for (float d=13;d<length;d+=5) {
                gfx_circle(x+(int)(game->aim.x*d),y+(int)(game->aim.y*d),1,ACCENT,true);
            }
            int tx=x+(int)(game->aim.x*length), ty=y+(int)(game->aim.y*length);
            for (int side=-1;side<=1;side+=2) {
                float a=atan2f(game->aim.y,game->aim.x)+3.14159f+side*0.5f;
                gfx_line(tx,ty,tx+(int)(cosf(a)*7),ty+(int)(sinf(a)*7),ACCENT);
            }
        }
    }
    Body ball=game->bodies[10];
    int bx=(int)ball.position.x, by=(int)ball.position.y;
    gfx_circle(bx+1,by+2,4,RGB565(20,70,36),true);
    gfx_circle(bx,by,4,INK,true);
    gfx_circle(bx,by,1,BLACK,true);
    gfx_text_center(160,228,"A/C DISK   STICK AIM   D POWER   B SHOOT   K PAUSE",1,MUTED);
}

/** Launcher picture: one blue and one red disk with a ball between them. */
void soccer_icon(int cx, int cy) {
    gfx_circle(cx-11,cy+3,8,BLUE_DARK,true); gfx_circle(cx-11,cy+3,6,BLUE,true);
    gfx_circle(cx+13,cy-5,8,RED_DARK,true);  gfx_circle(cx+13,cy-5,6,RED,true);
    gfx_circle(cx+2,cy+8,4,INK,true); gfx_circle(cx+2,cy+8,1,BLACK,true);
}
