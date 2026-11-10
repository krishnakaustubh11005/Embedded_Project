/* Shade Duel drawing: sunset arena, silhouette fighters, hit effects and HUD.
 * Reads game state only and writes the SRAM framebuffer; no SPI here.
 */
#include <math.h>
#include <stdio.h>
#include "config.h"
#include "graphics/gfx.h"
#include "games/fighter/fighter.h"

#define GROUND ((int)FIGHTER_GROUND_Y)
static const uint16_t BODY=RGB565(12,8,20);        // Near-black silhouette.
static const uint16_t BODY_BACK=RGB565(40,26,52);   // Far limbs, slightly lighter for depth.

/** Paint the sky gradient, sun, two hill layers and the floor (about 0.7 ms of fills). */
static void arena(void) {
    gfx_gradient(0,0,DISPLAY_WIDTH,110,RGB565(36,18,72),RGB565(200,70,110));
    gfx_gradient(0,110,DISPLAY_WIDTH,GROUND-110,RGB565(200,70,110),RGB565(255,176,96));
    gfx_circle(160,124,38,RGB565(255,214,150),true);
    gfx_circle(160,124,30,RGB565(255,236,190),true);
    for (int x=0;x<DISPLAY_WIDTH;x++) {
        int far=150+(int)(16*sinf(x*0.031f)+9*sinf(x*0.077f+1.0f));
        int near=180+(int)(9*sinf(x*0.052f+2.0f)+5*sinf(x*0.13f));
        gfx_rect(x,far,1,GROUND-far,RGB565(132,56,98));
        gfx_rect(x,near,1,GROUND-near,RGB565(78,30,66));
    }
    gfx_gradient(0,GROUND,DISPLAY_WIDTH,DISPLAY_HEIGHT-GROUND,RGB565(46,20,44),RGB565(14,6,18));
    gfx_rect(0,GROUND,DISPLAY_WIDTH,1,RGB565(255,150,96));
}

/** Draw one silhouette from its skeleton: back limbs, body, front limbs, accents.
 * @param accent Player colour for headband, belt and eyes.
 * @param flash Draw the whole body white (hit flash).
 */
static void figure(const Skeleton *s, int facing, uint16_t accent, bool flash, float clock) {
    float k=s->scale;
    uint16_t back=flash?INK:BODY_BACK, body=flash?INK:BODY;
    gfx_capsule(s->shoulder.x,s->shoulder.y,s->elbow[1].x,s->elbow[1].y,3.2f*k,back);
    gfx_capsule(s->elbow[1].x,s->elbow[1].y,s->hand[1].x,s->hand[1].y,2.8f*k,back);
    gfx_capsule(s->hand[1].x,s->hand[1].y,s->hand[1].x,s->hand[1].y,3.4f*k,back);
    gfx_capsule(s->hip.x,s->hip.y,s->knee[1].x,s->knee[1].y,4.2f*k,back);
    gfx_capsule(s->knee[1].x,s->knee[1].y,s->ankle[1].x,s->ankle[1].y,3.4f*k,back);
    gfx_capsule(s->ankle[1].x,s->ankle[1].y,s->toe[1].x,s->toe[1].y,2.2f*k,back);
    float cx=(s->hip.x+s->shoulder.x)/2, cy=(s->hip.y+s->shoulder.y)/2;
    gfx_capsule(s->hip.x,s->hip.y,cx,cy,5.0f*k,body);                     // Waist.
    gfx_capsule(cx,cy,s->shoulder.x,s->shoulder.y,6.5f*k,body);           // Chest.
    gfx_capsule(s->shoulder.x,s->shoulder.y,s->head.x,s->head.y,2.6f*k,body);  // Neck.
    gfx_capsule(s->head.x,s->head.y,s->head.x,s->head.y,6.5f*k,body);     // Head.
    if (!flash) {
        // Headband with two tails fluttering behind the head.
        float wave=sinf(clock*12)*2*k;
        gfx_capsule(s->head.x-5*k,s->head.y-2*k,s->head.x+5*k,s->head.y-2*k,1.3f*k,accent);
        float bx=s->head.x-facing*5*k, by=s->head.y-2*k;
        gfx_capsule(bx,by,bx-facing*8*k,by+3*k+wave,1.1f*k,accent);
        gfx_capsule(bx,by,bx-facing*7*k,by+6*k-wave,1.0f*k,accent);
        gfx_rect((int)(s->head.x+facing*2*k)-(facing<0?2:0),(int)(s->head.y),k>0.7f?3:2,1,INK);  // Eye.
    }
    gfx_capsule(s->hip.x,s->hip.y,s->knee[0].x,s->knee[0].y,4.2f*k,body);
    gfx_capsule(s->knee[0].x,s->knee[0].y,s->ankle[0].x,s->ankle[0].y,3.4f*k,body);
    gfx_capsule(s->ankle[0].x,s->ankle[0].y,s->toe[0].x,s->toe[0].y,2.2f*k,body);
    if (!flash) gfx_capsule(s->hip.x-4*k,s->hip.y-2*k,s->hip.x+4*k,s->hip.y-2*k,1.6f*k,accent);  // Belt.
    gfx_capsule(s->shoulder.x,s->shoulder.y,s->elbow[0].x,s->elbow[0].y,3.2f*k,body);
    gfx_capsule(s->elbow[0].x,s->elbow[0].y,s->hand[0].x,s->hand[0].y,2.8f*k,body);
    gfx_capsule(s->hand[0].x,s->hand[0].y,s->hand[0].x,s->hand[0].y,3.4f*k,body);
}

/** Draw a soft floor shadow that shrinks as the fighter rises. */
static void ground_shadow(float x, float height, int shake) {
    int w=(int)(34*fmaxf(0.3f,1-height/140));
    gfx_round_rect((int)x-w/2+shake,GROUND+1,w,4,2,RGB565(20,8,22));
}

/** Draw a fading "motion smear" behind the striking limb during an attack. */
static void trail(const Fighter *f, float clock, uint16_t accent, int shake) {
    const MoveInfo *move=fighter_move(f->action);
    if (!move || f->action_time<move->active_start-0.06f || f->action_time>move->active_end) return;
    for (unsigned int i=3;i>=1;i--) {
        Fighter past=*f; past.action_time=fmaxf(0,f->action_time-0.03f*i);
        Pose pose; Skeleton s; Vec2 point;
        fighter_pose(&past,clock,&pose);
        fighter_skeleton(&pose,past.x+shake,FIGHTER_GROUND_Y,past.height,past.facing,1,&s);
        point=move->foot?s.ankle[0]:s.hand[0];
        gfx_capsule(point.x,point.y,point.x,point.y,5.0f-i,gfx_mix(accent,RGB565(255,200,140),60*i));
    }
}

/** Burst of rays at the contact point: gold for clean hits, cyan for blocks. */
static void spark(const FighterGame *game, int shake) {
    if (game->spark_time<=0) return;
    float progress=1-game->spark_time/0.18f;
    float radius=4+14*progress;
    uint16_t color=game->spark_blocked?CYAN:ACCENT;
    int x=(int)game->spark.x+shake, y=(int)game->spark.y;
    for (unsigned int i=0;i<8;i++) {
        float a=i*0.785398f+0.3f, c=cosf(a), s=sinf(a);
        gfx_line(x+(int)(c*radius*0.4f),y+(int)(s*radius*0.4f),x+(int)(c*radius),y+(int)(s*radius),color);
    }
    if (progress<0.4f) gfx_circle(x,y,(int)(5-progress*8),INK,true);
}

/** Health bar with a lagging light segment showing recent damage. */
static void health_bar(const Fighter *f, unsigned int p) {
    const int w=124, x=p?DISPLAY_WIDTH-8-w:8;
    gfx_round_rect(x-2,5,w+4,12,4,SHADOW);
    gfx_rect(x,7,w,8,RGB565(60,20,36));
    int shown=(int)(w*f->health_shown/FIGHTER_MAX_HEALTH), now=(int)(w*f->health/FIGHTER_MAX_HEALTH);
    uint16_t fill=p?RED:BLUE, light=p?RGB565(255,150,150):RGB565(150,200,255);
    if (p) { gfx_rect(x+w-shown,7,shown,8,ACCENT); gfx_gradient(x+w-now,7,now,8,light,fill); }
    else   { gfx_rect(x,7,shown,8,ACCENT);         gfx_gradient(x,7,now,8,light,fill); }
}

/** Two energy cells per player; a full cell (enough for one special) glows gold. */
static void energy_bar(const Fighter *f, unsigned int p) {
    for (unsigned int cell=0;cell<2;cell++) {
        int x=p?DISPLAY_WIDTH-8-30-(int)cell*32:8+(int)cell*32;
        float amount=physics_clamp((f->energy-cell*FIGHTER_SPECIAL_COST)/FIGHTER_SPECIAL_COST,0,1);
        uint16_t frame=f->denied>0 && ((int)(f->denied*10)&1)?RED:SHADOW;
        gfx_round_rect(x-1,27,32,7,3,frame);
        gfx_rect(x+1,29,28,3,RGB565(30,30,60));
        int w=(int)(28*amount);
        gfx_rect(p?x+29-w:x+1,29,w,3,amount>=1?ACCENT:CYAN);
    }
}

/** Player tag and round-win pips under each health bar. */
static void round_pips(const FighterGame *game, unsigned int p) {
    int x=p?DISPLAY_WIDTH-8-gfx_text_width("P2",1):8;
    gfx_text_shadow(x,19,p?"P2":"P1",1,INK,SHADOW);
    for (unsigned int i=0;i<FIGHTER_ROUNDS_TO_WIN;i++) {
        int cx=p?x-8-(int)i*9:x+gfx_text_width("P1",1)+8+(int)i*9;
        gfx_circle(cx,21,3,SHADOW,true);
        if (game->rounds[p]>i) gfx_circle(cx,21,2,ACCENT,true);
    }
}

/** Timer, round number and large announcer messages. */
static void hud(const FighterGame *game) {
    for (unsigned int p=0;p<2;p++) {
        health_bar(&game->fighter[p],p); energy_bar(&game->fighter[p],p); round_pips(game,p);
    }
    gfx_round_rect(144,3,32,24,7,SHADOW);
    unsigned int seconds=(unsigned int)ceilf(game->time_left);
    char text[24];
    snprintf(text,sizeof(text),"%u",seconds);
    gfx_text_center(160,8,text,3,seconds<=10?RED:INK);
    snprintf(text,sizeof(text),"ROUND %u",game->round);
    gfx_text_center_shadow(160,30,text,1,INK,SHADOW);
    if (game->phase==DUEL_INTRO) {
        if (game->phase_time<1.0f) gfx_text_center_shadow(160,74,text,4,INK,SHADOW);
        else gfx_text_center_shadow(160,70,"FIGHT!",6,ACCENT,RED_DARK);
    } else if (game->phase==DUEL_ROUND_END) {
        if (game->phase_time<1.2f) {
            if (game->knockout) gfx_text_center_shadow(160,66,"K.O.!",6,RED,SHADOW);
            else gfx_text_center_shadow(160,70,"TIME!",6,ACCENT,SHADOW);
        } else {
            const char *who=game->round_winner==0?"P1 WINS":(game->round_winner==1?"P2 WINS":"DRAW");
            uint16_t color=game->round_winner==0?BLUE:(game->round_winner==1?RED:ACCENT);
            gfx_text_center_shadow(160,74,who,5,color,SHADOW);
        }
    }
}

/** Compose the whole fight scene; screen shake offsets fighters and effects only. */
void fighter_draw(const void *state) {
    const FighterGame *game=state;
    arena();
    int shake=game->shake>0?(int)(sinf(game->clock*90)*3*game->shake/0.22f):0;
    for (unsigned int p=0;p<2;p++) ground_shadow(game->fighter[p].x,game->fighter[p].height,shake);
    // Draw the attacker last so its strike overlaps the defender.
    unsigned int first=fighter_move(game->fighter[1].action)?0:1;
    for (unsigned int i=0;i<2;i++) {
        unsigned int p=i==0?first:first^1u;
        const Fighter *f=&game->fighter[p];
        uint16_t accent=p?RED:BLUE;
        trail(f,game->clock,accent,shake);
        Pose pose; Skeleton s;
        fighter_pose(f,game->clock,&pose);
        fighter_skeleton(&pose,f->x+shake,FIGHTER_GROUND_Y,f->height,f->facing,1,&s);
        figure(&s,f->facing,accent,f->flash>0,game->clock);
    }
    spark(game,shake);
    hud(game);
    gfx_text_center(160,226,"A PUNCH  B KICK  C BLOCK  D BRAZIL  K PAUSE",1,RGB565(200,150,170));
}

/** Launcher picture: blue fighter mid Brazilian kick against a red guard. */
void fighter_icon(int cx, int cy) {
    Fighter f={0}; Pose pose; Skeleton s;
    f.action=ACT_BRAZIL; f.action_time=0.36f;
    fighter_pose(&f,0,&pose);
    fighter_skeleton(&pose,(float)cx-10,(float)cy+20,0,1,0.5f,&s);
    figure(&s,1,BLUE,false,0);
    f.action=ACT_IDLE; f.action_time=0;
    fighter_pose(&f,0,&pose);
    fighter_skeleton(&pose,(float)cx+12,(float)cy+20,0,-1,0.5f,&s);
    figure(&s,-1,RED,false,0);
}
