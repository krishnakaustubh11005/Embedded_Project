/* Shade Duel rules: per-fighter state machine, hit detection and round flow.
 *
 * Each physics tick: buffered button presses and held stick directions choose
 * actions, motion is integrated, fighters are kept apart, then the active
 * attack limb (taken from the animated skeleton) is tested against the
 * opponent's body box. No GPIO, SPI, timers or allocation are used here.
 */
#include <math.h>
#include <string.h>
#include "config.h"
#include "graphics/gfx.h"
#include "audio/sound.h"
#include "games/fighter/fighter.h"

#define STRIKE_RADIUS 7.0f
#define BODY_HALF_WIDTH 11.0f
#define HIT_SECONDS 0.28f
#define ROLL_SECONDS 0.42f
#define KNOCKDOWN_SECONDS 1.45f
#define INTRO_SECONDS 1.8f
#define ROUND_END_SECONDS 3.0f

/* duration, active window, damage, push (px/s), height, foot?, knockdown? */
static const MoveInfo moves[ACT_COUNT]={
    [ACT_PUNCH]   ={0.30f,0.07f,0.17f, 6, 60,HIT_HIGH,false,false},
    [ACT_UPPERCUT]={0.50f,0.14f,0.26f,11, 90,HIT_MID, false,true },
    [ACT_KICK]    ={0.42f,0.18f,0.28f, 9,110,HIT_MID, true, false},
    [ACT_SWEEP]   ={0.55f,0.18f,0.32f, 8, 40,HIT_LOW, true, true },
    [ACT_FLY_KICK]={9.00f,0.06f,9.00f,10,120,HIT_MID, true, false},  // Ends on landing.
    [ACT_BRAZIL]  ={0.72f,0.28f,0.42f,20,160,HIT_HIGH,true, true }};

/** Return the attack table entry, or NULL when the action deals no damage. */
const MoveInfo *fighter_move(FighterAction action) {
    return action<ACT_COUNT && moves[action].duration>0?&moves[action]:NULL;
}

/** Switch action and restart its animation clock; clears the "already hit" flag. */
static void start(Fighter *fighter, FighterAction action) {
    fighter->action=action; fighter->action_time=0; fighter->hit_landed=false;
}

/** Crouching fighters (including low block and sweep) duck under HIGH attacks. */
bool fighter_is_low(const Fighter *fighter) {
    return fighter->action==ACT_CROUCH || fighter->action==ACT_CROUCH_BLOCK ||
           fighter->action==ACT_SWEEP || fighter->action==ACT_ROLL;
}

/** True when standing on the floor in a neutral state and able to start a move. */
static bool can_act(const Fighter *fighter) {
    bool neutral=fighter->action==ACT_IDLE || fighter->action==ACT_WALK || fighter->action==ACT_CROUCH ||
                 fighter->action==ACT_BLOCK || fighter->action==ACT_CROUCH_BLOCK;
    return neutral && fighter->stun<=0 && fighter->height<=0;
}

/** Place both fighters for a new round with full health and empty energy. */
static void place_fighters(FighterGame *game) {
    for (unsigned int p=0;p<2;p++) {
        Fighter *f=&game->fighter[p];
        memset(f,0,sizeof(*f));
        f->x=p?220:100; f->facing=p?-1:1;
        f->health=f->health_shown=FIGHTER_MAX_HEALTH;
        f->action=ACT_IDLE;
    }
    game->time_left=FIGHTER_ROUND_SECONDS;
    game->phase=DUEL_INTRO; game->phase_time=0;
    game->spark_time=game->shake=game->hitstop=0;
    game->knockout=false; game->round_winner=-1;
    sound_play(SND_DUEL_ROUND);  // Taiko and gong under "ROUND n".
}

/** Start a best-of-three match in caller-owned RAM; the seed is unused. */
void fighter_reset(void *state, uint32_t seed) {
    (void)seed;
    FighterGame *game=state;
    memset(game,0,sizeof(*game));
    game->round=1; game->winner=-1;
    place_fighters(game);
}

/** Remember each player's newest press (priority D, B, A, C) for a short time.
 * @note Called once per rendered frame. Buffering lets a press made slightly
 *       before the current move ends still trigger the next move.
 */
void fighter_input(void *state, const InputSnapshot *snapshot) {
    FighterGame *game=state;
    static const uint8_t priority[]={KEY_D,KEY_B,KEY_A,KEY_C};
    for (unsigned int p=0;p<2;p++) {
        uint8_t pressed=snapshot->player[p].pressed;
        for (unsigned int i=0;i<4;i++) {
            if (pressed&priority[i]) {
                game->fighter[p].buffered=priority[i];
                game->fighter[p].buffer_time=FIGHTER_INPUT_BUFFER;
                break;
            }
        }
    }
}

/** Turn stick direction and buffered buttons into a new action for player p.
 * @note Axes are right/up positive. "Forward" is relative to facing, so the
 *       same stick motion means advance for one player and retreat for the other.
 */
static void control(FighterGame *game, unsigned int p, const Controller *controller, float seconds) {
    Fighter *f=&game->fighter[p];
    const Fighter *other=&game->fighter[p^1u];
    bool fighting=game->phase==DUEL_FIGHT;
    float sx=fighting?controller->x:0, sy=fighting?controller->y:0;
    uint8_t held=fighting?controller->held:0;
    if (!fighting) f->buffered=0;
    if (f->buffer_time>0) f->buffer_time-=seconds;
    else f->buffered=0;
    if (f->action==ACT_JUMP && f->buffered==KEY_B) {
        start(f,ACT_FLY_KICK);
        sound_play(SND_DUEL_WHOOSH);
        f->vx=f->facing*fmaxf(fabsf(f->vx),110);
        f->buffered=0;
        return;
    }
    if (!can_act(f)) return;
    if (fabsf(other->x-f->x)>2) f->facing=other->x>f->x?1:-1;
    bool up=sy>FIGHTER_STICK_THRESHOLD, down=sy< -FIGHTER_STICK_THRESHOLD;
    bool forward=sx*f->facing>FIGHTER_STICK_THRESHOLD, back=sx*f->facing< -FIGHTER_STICK_THRESHOLD;
    if (f->buffered) {
        FighterAction next=ACT_COUNT;
        if (f->buffered==KEY_D) {
            if (f->energy>=FIGHTER_SPECIAL_COST) { next=ACT_BRAZIL; f->energy-=FIGHTER_SPECIAL_COST; }
            else { f->denied=0.6f; sound_play(SND_DUEL_DENIED); }
        } else if (f->buffered==KEY_B) next=down?ACT_SWEEP:ACT_KICK;
        else if (f->buffered==KEY_A) next=forward?ACT_UPPERCUT:ACT_PUNCH;
        else if (f->buffered==KEY_C && back) next=ACT_ROLL;
        f->buffered=0; f->buffer_time=0;
        if (next!=ACT_COUNT) {
            start(f,next);
            sound_play(next==ACT_BRAZIL?SND_DUEL_SPECIAL:SND_DUEL_WHOOSH);
            f->vx=next==ACT_ROLL?-f->facing*FIGHTER_ROLL_SPEED:0;
            return;
        }
    }
    if (up) {
        start(f,ACT_JUMP);
        sound_play(SND_DUEL_JUMP);
        f->vy=FIGHTER_JUMP_SPEED; f->height=0.01f;
        f->vx=forward?f->facing*FIGHTER_WALK_SPEED:(back?-f->facing*FIGHTER_BACK_SPEED:0);
        return;
    }
    FighterAction next=(held&KEY_C)?(down?ACT_CROUCH_BLOCK:ACT_BLOCK):
                       down?ACT_CROUCH:(forward||back)?ACT_WALK:ACT_IDLE;
    if (next!=f->action) { f->action=next; f->action_time=0; }
    f->vx=next==ACT_WALK?(forward?f->facing*FIGHTER_WALK_SPEED:-f->facing*FIGHTER_BACK_SPEED):0;
}

/** Integrate one fighter: timers, lunges, gravity, floor friction and action ends. */
static void advance(Fighter *f, float seconds) {
    f->action_time+=seconds;
    f->stun=fmaxf(0,f->stun-seconds);
    f->flash=fmaxf(0,f->flash-seconds);
    f->denied=fmaxf(0,f->denied-seconds);
    f->health_shown=f->health_shown>f->health?fmaxf(f->health,f->health_shown-35*seconds):f->health;
    float t=f->action_time;
    if (f->action==ACT_UPPERCUT && t>=0.10f && t<=0.24f) f->vx=f->facing*70.0f;
    if (f->action==ACT_BRAZIL && t>=0.08f && t<=0.40f) f->vx=f->facing*45.0f;
    if (f->height>0 || f->vy>0) {
        f->vy-=FIGHTER_GRAVITY*seconds;
        f->height+=f->vy*seconds;
        if (f->height<=0) {
            f->height=0; f->vy=0;
            if (f->action==ACT_JUMP || f->action==ACT_FLY_KICK) {
                if (f->action==ACT_FLY_KICK) f->stun=0.10f;  // Short landing recovery.
                start(f,ACT_IDLE); f->vx=0;
            }
        }
    } else if (f->action!=ACT_WALK && f->action!=ACT_ROLL) {
        float slow=700*seconds;  // Constant floor friction for pushback and slides.
        f->vx=fabsf(f->vx)<=slow?0:f->vx-copysignf(slow,f->vx);
    }
    f->x=physics_clamp(f->x+f->vx*seconds,FIGHTER_ARENA_LEFT,FIGHTER_ARENA_RIGHT);
    if (f->action==ACT_WALK) f->walk_phase+=fabsf(f->vx)*seconds*0.11f;
    const MoveInfo *move=fighter_move(f->action);
    float limit=move?move->duration:f->action==ACT_HIT?HIT_SECONDS:f->action==ACT_ROLL?ROLL_SECONDS:
                f->action==ACT_KNOCKDOWN?KNOCKDOWN_SECONDS:0;
    if (limit>0 && t>=limit && f->height<=0) { start(f,ACT_IDLE); f->vx=0; }
}

/** Keep standing fighters at least FIGHTER_MIN_SEPARATION apart, inside the arena. */
static void separate(FighterGame *game) {
    Fighter *a=&game->fighter[0], *b=&game->fighter[1];
    if (fabsf(a->height-b->height)>=40) return;  // A high jump may pass over.
    float dx=b->x-a->x, side=dx!=0?(dx>0?1.0f:-1.0f):(float)a->facing;
    float gap=FIGHTER_MIN_SEPARATION-fabsf(dx);
    if (gap<=0) return;
    a->x=physics_clamp(a->x-side*gap/2,FIGHTER_ARENA_LEFT,FIGHTER_ARENA_RIGHT);
    b->x=physics_clamp(b->x+side*gap/2,FIGHTER_ARENA_LEFT,FIGHTER_ARENA_RIGHT);
    if (fabsf(b->x-a->x)<FIGHTER_MIN_SEPARATION-0.01f) {  // One was pinned to a wall.
        if (a->x<=FIGHTER_ARENA_LEFT || a->x>=FIGHTER_ARENA_RIGHT) b->x=a->x+side*FIGHTER_MIN_SEPARATION;
        else a->x=b->x-side*FIGHTER_MIN_SEPARATION;
    }
}

/** Rolling (mid-roll), knocked-down and KO'd fighters cannot be hit. */
static bool invulnerable(const Fighter *f) {
    if (f->action==ACT_ROLL) return f->action_time>=0.04f && f->action_time<=0.32f;
    return f->action==ACT_KNOCKDOWN || f->action==ACT_KO;
}

/** Circle-versus-box overlap using the closest point on the box to the centre. */
static bool circle_hits_box(Vec2 centre, float radius, Rect box) {
    float nx=physics_clamp(centre.x,box.x,box.x+box.w), ny=physics_clamp(centre.y,box.y,box.y+box.h);
    float dx=centre.x-nx, dy=centre.y-ny;
    return dx*dx+dy*dy<=radius*radius;
}

/** Body box: hip-centred, from the top of the head down to the lowest foot. */
static Rect body_box(const Skeleton *s) {
    float bottom=fmaxf(fmaxf(s->ankle[0].y,s->ankle[1].y),fmaxf(s->toe[0].y,s->toe[1].y));
    float top=s->head.y-7;
    return (Rect){s->hip.x-BODY_HALF_WIDTH,top,2*BODY_HALF_WIDTH,bottom-top};
}

/** Apply a confirmed contact from attacker p, including block, knockdown and KO.
 * @note Blocking needs the right height: standing block stops HIGH/MID,
 *       crouching block stops LOW/MID. Blocked hits do 15% chip damage.
 */
static void apply_hit(FighterGame *game, unsigned int p, Vec2 point) {
    Fighter *a=&game->fighter[p], *d=&game->fighter[p^1u];
    const MoveInfo *m=fighter_move(a->action);
    float push=(float)a->facing;
    a->hit_landed=true;
    bool facing_attacker=d->facing==(a->x>d->x?1:-1);
    bool blocked=d->height<=0 && facing_attacker &&
        ((d->action==ACT_BLOCK && m->height!=HIT_LOW) || (d->action==ACT_CROUCH_BLOCK && m->height!=HIT_HIGH));
    game->spark=point; game->spark_time=0.18f; game->spark_blocked=blocked;
    if (blocked) {
        d->health-=m->damage*0.15f;
        d->stun=0.16f; d->vx=push*m->push*0.8f;
        d->energy+=4; game->hitstop=0.04f;
        sound_play(SND_DUEL_BLOCK);
    } else {
        d->health-=m->damage; d->flash=0.12f; d->stun=0; d->buffered=0;
        a->energy+=m->damage*1.4f; d->energy+=m->damage*0.5f;
        bool heavy=m->knockdown || d->health<=0 || d->height>0;
        if (d->health<=0) { start(d,ACT_KO); d->vy=150; d->height=fmaxf(d->height,0.01f); }
        else if (heavy) start(d,ACT_KNOCKDOWN);
        else start(d,ACT_HIT);
        d->vx=push*m->push;
        game->hitstop=heavy?0.09f:0.05f;
        game->shake=heavy?0.22f:0.07f;
        sound_play(heavy?SND_DUEL_HEAVY_HIT:m->foot?SND_DUEL_KICK_HIT:SND_DUEL_PUNCH_HIT);
        if (d->health<=0) sound_play(SND_DUEL_KO);
    }
    if (d->health<0) d->health=0;
    for (unsigned int i=0;i<2;i++) game->fighter[i].energy=fminf(game->fighter[i].energy,FIGHTER_ENERGY_MAX);
}

/** Test both attackers against the poses at the same instant, so trades are fair. */
static void resolve_hits(FighterGame *game) {
    Skeleton skeleton[2];
    for (unsigned int p=0;p<2;p++) {
        Pose pose; const Fighter *f=&game->fighter[p];
        fighter_pose(f,game->clock,&pose);
        fighter_skeleton(&pose,f->x,FIGHTER_GROUND_Y,f->height,f->facing,1,&skeleton[p]);
    }
    bool hit[2]={false,false}; Vec2 point[2];
    for (unsigned int p=0;p<2;p++) {
        const Fighter *a=&game->fighter[p], *d=&game->fighter[p^1u];
        if (a->hit_landed || !fighter_strike(a,&skeleton[p],&point[p]) || invulnerable(d)) continue;
        if (fighter_move(a->action)->height==HIT_HIGH && fighter_is_low(d)) continue;  // Ducked.
        hit[p]=circle_hits_box(point[p],STRIKE_RADIUS,body_box(&skeleton[p^1u]));
    }
    for (unsigned int p=0;p<2;p++) if (hit[p]) apply_hit(game,p,point[p]);
}

/** Close the round: KO or timeout winner by remaining health; a tie scores both. */
static void end_round(FighterGame *game, bool knockout) {
    float h0=game->fighter[0].health, h1=game->fighter[1].health;
    game->round_winner=h0>h1?0:(h1>h0?1:-1);
    if (game->round_winner<0) { game->rounds[0]++; game->rounds[1]++; }
    else game->rounds[game->round_winner]++;
    game->knockout=knockout; game->phase=DUEL_ROUND_END; game->phase_time=0;
}

/** Advance one fixed step: input, motion, separation, hits and round timing.
 * @note A short "hit-stop" freezes fighters for a few ticks after contact so
 *       impacts read clearly; announcer and effect timers keep running.
 */
void fighter_update(void *state, const InputSnapshot *snapshot, float seconds) {
    FighterGame *game=state;
    if (game->over || seconds<=0) return;
    game->clock+=seconds; game->phase_time+=seconds;
    game->spark_time=fmaxf(0,game->spark_time-seconds);
    game->shake=fmaxf(0,game->shake-seconds);
    if (game->hitstop>0) { game->hitstop-=seconds; return; }
    for (unsigned int p=0;p<2;p++) control(game,p,&snapshot->player[p],seconds);
    for (unsigned int p=0;p<2;p++) advance(&game->fighter[p],seconds);
    separate(game);
    if (game->phase==DUEL_FIGHT) resolve_hits(game);
    switch (game->phase) {
        case DUEL_INTRO:
            if (game->phase_time>=INTRO_SECONDS) {
                game->phase=DUEL_FIGHT; game->phase_time=0;
                sound_play(SND_DUEL_FIGHT);
            }
            break;
        case DUEL_FIGHT:
            game->time_left-=seconds;
            if (game->fighter[0].health<=0 || game->fighter[1].health<=0) end_round(game,true);
            else if (game->time_left<=0) {
                game->time_left=0; end_round(game,false);
                sound_play(SND_DUEL_ROUND);
            }
            break;
        case DUEL_ROUND_END:
            if (game->phase_time>0.9f && game->round_winner>=0) {
                Fighter *w=&game->fighter[game->round_winner];
                if (can_act(w) && w->action!=ACT_VICTORY) start(w,ACT_VICTORY);
            }
            if (game->phase_time>=ROUND_END_SECONDS) {
                if (game->rounds[0]>=FIGHTER_ROUNDS_TO_WIN || game->rounds[1]>=FIGHTER_ROUNDS_TO_WIN) {
                    game->winner=game->rounds[0]>game->rounds[1]?0:(game->rounds[1]>game->rounds[0]?1:-1);
                    game->phase=DUEL_OVER; game->over=true;
                } else { game->round++; place_fighters(game); }
            }
            break;
        case DUEL_OVER: break;
    }
}

/** Report match completion to the shared results screen. */
static bool finished(const void *state) { return ((const FighterGame *)state)->over; }

/** Return the match winner, or -1 for a draw. */
static int winner(const void *state) { return ((const FighterGame *)state)->winner; }

/** Return a constant winner label. */
static const char *result(const void *state) {
    int w=winner(state);
    return w==0?"PLAYER 1 WINS":(w==1?"PLAYER 2 WINS":"DRAW GAME");
}

const GameSpec fighter_spec={"SHADE DUEL","2 PLAYERS - FIGHT",
    {"STICK WALKS, UP JUMPS, DOWN CROUCHES","A PUNCH. FORWARD+A UPPERCUT.",
     "B KICK. DOWN+B SWEEP. AIR B FLY KICK","HOLD C BLOCK. DOWN+C BLOCKS SWEEPS.",
     "BACK+C ROLLS AWAY FROM ATTACKS.","D BRAZILIAN KICK: COSTS 1 ENERGY BAR",
     "CROUCH UNDER HIGH HITS. WIN 2 ROUNDS"},
    fighter_reset,fighter_input,fighter_update,fighter_draw,finished,result,
    ROSE,fighter_icon,winner,MUSIC_NINJA,true};
