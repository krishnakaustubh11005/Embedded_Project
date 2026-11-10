/* Console UI: launcher tiles, rules, pause/results pop-ups and real input diagnostics.
 * Visual style: bright flat colours, rounded cards, P1 blue versus P2 red,
 * drop-shadowed titles. Everything is drawn into SRAM; no SPI here.
 */
#include <stdio.h>
#include "config.h"
#include "graphics/gfx.h"
#include "app/app.h"

/** Draw the common top bar with a shadowed title over a soft gradient. */
static void heading(const char *text) {
    gfx_clear(BACKGROUND);
    gfx_gradient(0,0,DISPLAY_WIDTH,34,PANEL_LIGHT,BACKGROUND);
    gfx_text_shadow(12,7,text,3,INK,SHADOW);
}

/** Draw a rounded "pill" button with centred text. */
static void pill(int x, int y, int w, const char *text, uint16_t fill, uint16_t ink) {
    gfx_round_rect(x,y+2,w,16,8,SHADOW);
    gfx_round_rect(x,y,w,16,8,fill);
    gfx_text_center(x+w/2,y+3,text,2,ink);
}

/** Small P1/P2 badges in the top-right corner of the launcher. */
static void player_badges(void) {
    gfx_round_rect(238,9,32,16,8,BLUE); gfx_text_center(254,12,"P1",2,INK);
    gfx_round_rect(276,9,32,16,8,RED);  gfx_text_center(292,12,"P2",2,INK);
}

/** Render one launcher tile: colour card, title, tagline, player count and icon.
 * @note Unselected tiles are blended toward the background so the cursor is
 *       obvious; the selected tile gets a white frame and is lifted 2 px.
 */
static void tile(int x, int y, const GameSpec *spec, bool selected) {
    const int w=146, h=74;
    uint16_t base=selected?spec->color:gfx_mix(spec->color,BACKGROUND,110);
    if (selected) { y-=2; gfx_round_rect(x-3,y-3,w+6,h+6,12,INK); }
    else gfx_round_rect(x,y+3,w,h,10,SHADOW);
    gfx_round_rect(x,y,w,h,10,base);
    gfx_round_rect(x+3,y+3,w-6,h/2-3,8,gfx_mix(base,INK,40));
    gfx_text_shadow(x+8,y+7,spec->name,2,INK,SHADOW);
    gfx_text(x+8,y+22,spec->tagline,1,selected?INK:gfx_mix(INK,base,90));
    bool two_player=spec->tagline[0]=='2';
    gfx_round_rect(x+8,y+h-19,26,13,6,SHADOW);
    gfx_text_center(x+21,y+h-16,two_player?"2P":"1P",1,two_player?ACCENT:GREEN);
    spec->icon(x+w-38,y+h-30);
}

/** Render the launcher grid and diagnostics entry; P1 A/C moves the cursor. */
static void menu(const App *app) {
    heading("PICO ARCADE");
    player_badges();
    for (unsigned int i=0;i<GAME_COUNT;i++) {
        int x=i%2?164:10, y=i<2?42:122;
        tile(x,y,game_get(i),app->selected==i);
    }
    bool diag=app->selected==GAME_COUNT;
    if (diag) gfx_round_rect(7,199,306,22,10,INK);
    gfx_round_rect(10,201,300,18,9,diag?CYAN:PANEL);
    gfx_text_center(160,205,"CONTROLLER DIAGNOSTICS",2,diag?SHADOW:MUTED);
    gfx_text_center(160,228,"A/C MOVE    B SELECT    D BACK",1,MUTED);
}

/** Render every rule at once under a game-coloured banner. */
static void rules(const App *app) {
    const GameSpec *spec=game_get(app->selected);
    gfx_clear(BACKGROUND);
    gfx_gradient(0,0,DISPLAY_WIDTH,40,spec->color,gfx_mix(spec->color,BACKGROUND,150));
    gfx_text_shadow(12,9,spec->name,3,INK,SHADOW);
    gfx_text(12,30,"HOW TO PLAY",1,INK);
    spec->icon(270,18);
    gfx_round_rect(8,48,304,152,10,PANEL);
    for (unsigned int i=0;i<7;i++) {
        if (!spec->rules[i]) continue;
        int y=56+(int)i*20;
        gfx_circle(14,y+4,2,spec->color,true);
        gfx_text(21,y,spec->rules[i],2,INK);
    }
    if (spec->sound_themes) {
        /* Third pill: C cycles the sound theme; the name shows the current one. */
        char label[16];
        snprintf(label,sizeof(label),"C %s",sound_theme_name(sound_duel_theme()));
        gfx_text_center(160,203,"SOUND THEME",1,MUTED);
        pill(8,212,96,"B START",GREEN,SHADOW);
        pill(112,212,96,label,sound_duel_theme()==DUEL_THEME_CLIPS?ACCENT:PURPLE,
             sound_duel_theme()==DUEL_THEME_CLIPS?SHADOW:INK);
        pill(216,212,96,"D BACK",PANEL_LIGHT,INK);
        return;
    }
    pill(20,212,120,"B START",GREEN,SHADOW);
    pill(180,212,120,"D BACK",PANEL_LIGHT,INK);
}

/** One controller card: stick position, raw/centre/axis values and buttons. */
static void controller_card(int x, unsigned int p, const Controller *c) {
    uint16_t color=p?RED:BLUE;
    char line[40];
    gfx_round_rect(x,40,148,122,8,PANEL);
    gfx_round_rect(x,40,148,18,8,color); gfx_rect(x,50,148,8,color);
    snprintf(line,sizeof(line),"PLAYER %u",p+1);
    gfx_text_shadow(x+8,44,line,2,INK,SHADOW);
    int cx=x+30, cy=92;
    gfx_circle(cx,cy,22,PANEL_LIGHT,true); gfx_circle(cx,cy,22,MUTED,false);
    gfx_rect(cx-22,cy,45,1,MUTED); gfx_rect(cx,cy-22,1,45,MUTED);
    gfx_circle(cx+(int)(c->x*17),cy-(int)(c->y*17),5,color,true);
    gfx_circle(cx+(int)(c->x*17),cy-(int)(c->y*17),5,INK,false);
    snprintf(line,sizeof(line),"X %4u",c->raw[0]); gfx_text(x+60,66,line,2,INK);
    snprintf(line,sizeof(line),"Y %4u",c->raw[1]); gfx_text(x+60,80,line,2,INK);
    snprintf(line,sizeof(line),"CTR %4u %4u",c->centre[0],c->centre[1]); gfx_text(x+60,96,line,1,MUTED);
    snprintf(line,sizeof(line),"AXIS %.2f %.2f",(double)c->x,(double)c->y); gfx_text(x+60,106,line,1,MUTED);
    static const char *const names[]={"A","B","C","D","K"};
    for (unsigned int bit=0;bit<5;bit++) {
        int bx=x+18+(int)bit*28;
        bool held=(c->held&(1u<<bit))!=0;
        gfx_circle(bx,140,9,held?color:PANEL_LIGHT,true);
        gfx_text_center(bx,136,names[bit],2,held?INK:MUTED);
    }
}

/** Draw sampled values and timing, with no simulated readings or presence claims. */
static void diagnostics(const InputSnapshot *input, const PerfReport *report) {
    heading("DIAGNOSTICS");
    for (unsigned int p=0;p<2;p++) controller_card(p?164:8,p,&input->player[p]);
    char line[64];
    snprintf(line,sizeof(line),"%.1f FPS  SPI %u HZ",(double)report->fps,report->spi_hz);
    gfx_text(12,168,line,2,CYAN);
    snprintf(line,sizeof(line),"DRAW %u  TX %u US  GAP %u US",report->average_us[2],report->average_us[3],report->max_gap_us);
    gfx_text(12,186,line,1,MUTED);
    snprintf(line,sizeof(line),"CAL %u/%u  CENTRE VALID %u/15",input->calibration_samples,INPUT_CALIBRATION_SAMPLES,input->centre_valid);
    gfx_text(12,198,line,1,input->centre_valid==15?GREEN:ACCENT);
    pill(20,214,140,"A RECALIBRATE",ACCENT,SHADOW);
    pill(200,214,100,"D BACK",PANEL_LIGHT,INK);
}

/** Boot screen shown while both joystick centres are sampled. */
static void boot(const InputSnapshot *input) {
    gfx_gradient(0,0,DISPLAY_WIDTH,DISPLAY_HEIGHT,PANEL_LIGHT,BACKGROUND);
    gfx_text_center_shadow(160,40,"PICO ARCADE",5,INK,SHADOW);
    gfx_round_rect(84,78,70,18,9,BLUE); gfx_text_center(119,82,"P1",2,INK);
    gfx_round_rect(166,78,70,18,9,RED); gfx_text_center(201,82,"P2",2,INK);
    gfx_text_center(160,116,"RELEASE BOTH JOYSTICKS",2,INK);
    gfx_text_center(160,136,"CALIBRATING CENTRES",2,CYAN);
    gfx_round_rect(40,160,240,14,7,SHADOW);
    int fill=(int)(236*input->calibration_samples/INPUT_CALIBRATION_SAMPLES);
    if (fill>0) gfx_round_rect(42,162,fill,10,5,ACCENT);
    gfx_text_center(160,206,"ST7789 - TWO CONTROLLERS",1,MUTED);
}

/** Dim the frozen game and draw the pause card with three choices. */
static void pause_overlay(const App *app) {
    static const char *const choices[]={"RESUME","RESTART","QUIT TO MENU"};
    gfx_dim(0,0,DISPLAY_WIDTH,DISPLAY_HEIGHT);
    gfx_round_rect(60,40,200,160,14,PANEL);
    gfx_text_center_shadow(160,52,"PAUSED",3,INK,SHADOW);
    for (unsigned int i=0;i<3;i++) {
        bool on=app->pause_choice==i;
        int y=86+(int)i*28;
        gfx_round_rect(80,y,160,22,11,on?ACCENT:PANEL_LIGHT);
        gfx_text_center(160,y+6,choices[i],2,on?SHADOW:INK);
    }
    gfx_text_center(160,180,"A/C MOVE  B SELECT  D RESUME",1,MUTED);
}

/** Dim the final game state and show the winner in that player's colour. */
static void results_overlay(const GameSpec *spec, const void *state) {
    int winner=spec->winner(state);
    uint16_t color=winner==0?BLUE:(winner==1?RED:spec->color);
    gfx_dim(0,0,DISPLAY_WIDTH,DISPLAY_HEIGHT);
    gfx_round_rect(28,58,264,124,14,PANEL);
    gfx_round_rect(28,58,264,46,14,color); gfx_rect(28,90,264,14,color);
    gfx_text_center_shadow(160,72,spec->result(state),3,INK,SHADOW);
    gfx_text_center(160,114,winner>=0?"WELL PLAYED!":"GOOD GAME!",2,ACCENT);
    pill(44,148,108,"B REMATCH",GREEN,SHADOW);
    pill(168,148,108,"D MENU",PANEL_LIGHT,INK);
}

/** Compose one application screen; game rendering and overlays only touch SRAM. */
void app_draw(const App *app, const InputSnapshot *input, const PerfReport *report) {
    if (app->screen==SCREEN_BOOT) { boot(input); return; }
    if (app->screen==SCREEN_MENU) { menu(app); return; }
    if (app->screen==SCREEN_RULES) { rules(app); return; }
    if (app->screen==SCREEN_DIAGNOSTICS) { diagnostics(input,report); return; }
    const GameSpec *spec=game_get(app->selected);
    spec->draw(&app->game);
    if (app->screen==SCREEN_PAUSE) pause_overlay(app);
    else if (app->screen==SCREEN_RESULTS) results_overlay(spec,&app->game);
}
