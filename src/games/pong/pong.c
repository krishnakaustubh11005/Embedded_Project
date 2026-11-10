/* Automatic-contact two-axis Pong with continuous face/corner collision tests. */
#include <math.h>
#include <string.h>
#include "config.h"
#include "graphics/gfx.h"
#include "audio/sound.h"
#include "games/pong/pong.h"

/** Reset ball/paddle positions and begin a one-second nonblocking serve toward direction. */
static void begin_serve(PongGame *game, float direction) {
    game->paddle[0]=(Rect){32,110,8,36};
    game->paddle[1]=(Rect){280,110,8,36};
    game->paddle_velocity[0]=game->paddle_velocity[1]=(Vec2){0,0};
    game->ball=(Body){{160,128},{direction*PONG_INITIAL_SPEED*0.9539392f,PONG_INITIAL_SPEED*0.3f},4,1};
    game->serve_seconds=1;
}

/** Reset match scores and start serving; seed selects the initial left/right direction. */
void pong_reset(void *state, uint32_t seed) {
    PongGame *game=state;
    memset(game,0,sizeof(*game));
    begin_serve(game,(seed&1u)?1:-1);
}

/** Award a point; either finish at seven or reset the next serve toward the conceding player. */
static void point(PongGame *game, unsigned int player) {
    if (++game->score[player]>=PONG_WIN_SCORE) {
        game->over=true; game->winner=player;
        game->ball.velocity=(Vec2){0,0};
    } else { sound_play(SND_PONG_SCORE); begin_serve(game,player==0?1:-1); }
}

/** Enforce a speed cap and nonzero horizontal travel so rallies do not stall vertically. */
static void limit_ball(PongGame *game) {
    Vec2 *v=&game->ball.velocity;
    *v=physics_limit(*v,PONG_MAX_SPEED);
    if (fabsf(v->x)<60) v->x=copysignf(60,v->x);
    float max_y=sqrtf(fmaxf(0,PONG_MAX_SPEED*PONG_MAX_SPEED-v->x*v->x));
    v->y=physics_clamp(v->y,-max_y,max_y);
}

/** Bounce from a moving paddle: face impact height sets aim; corners use their normal.
 * @param box Paddle at impact, velocity its measured motion after boundary clamping.
 * @note Only called for approaching contact. Adds a small motion influence, then caps speed.
 */
static void paddle_bounce(PongGame *game, Rect box, Vec2 velocity, Vec2 normal) {
    Vec2 *ball=&game->ball.velocity;
    sound_play(SND_PONG_PADDLE);
    if (fabsf(normal.x)>0.999f && fabsf(normal.y)<0.001f) {
        float offset=physics_clamp((game->ball.position.y-box.y-box.h/2)/(box.h/2),-1,1);
        float angle=offset*1.04719755f;
        float speed=physics_clamp(physics_length(*ball)*1.03f,180,PONG_MAX_SPEED);
        ball->x=normal.x*speed*cosf(angle)+velocity.x*0.25f;
        ball->y=speed*sinf(angle)+velocity.y*0.25f;
        if (ball->x*normal.x<60) ball->x=normal.x*60;
    } else {
        Vec2 relative={ball->x-velocity.x,ball->y-velocity.y};
        float approach=relative.x*normal.x+relative.y*normal.y;
        ball->x-=2*approach*normal.x;
        ball->y-=2*approach*normal.y;
    }
    limit_ball(game);
    game->hits++;
}

/** Sweep a ball through one physics interval, including moving paddles and arena boundaries.
 * @param old Paddle positions before this interval; velocities are constant during it.
 * @return One/two for a goal scored by player one/two, or zero; loops are bounded.
 */
static unsigned int advance_ball(PongGame *game, const Rect old[2], float seconds) {
    float remaining=seconds;
    for (unsigned int iteration=0;iteration<6 && remaining>0.000001f;iteration++) {
        float earliest=1;
        int event=-1;  // 0/1 paddles, 2/3 top/bottom, 4/5 left/right goal.
        Vec2 normal={0,0};
        float used=seconds-remaining;
        for (unsigned int p=0;p<2;p++) {
            Rect box=old[p];
            box.x+=game->paddle_velocity[p].x*used; box.y+=game->paddle_velocity[p].y*used;
            Vec2 relative={(game->ball.velocity.x-game->paddle_velocity[p].x)*remaining,
                           (game->ball.velocity.y-game->paddle_velocity[p].y)*remaining};
            SweepHit hit;
            if (physics_sweep_rect(game->ball.position,relative,game->ball.radius,box,&hit) && hit.time<=earliest) {
                earliest=hit.time; event=(int)p; normal=hit.normal;
            }
        }
        Vec2 position=game->ball.position, v=game->ball.velocity;
        float times[4]={2,2,2,2};
        if (v.y<0) times[0]=(40-position.y)/(v.y*remaining);
        if (v.y>0) times[1]=(216-position.y)/(v.y*remaining);
        if (v.x<0) times[2]=(4-position.x)/(v.x*remaining);
        if (v.x>0) times[3]=(316-position.x)/(v.x*remaining);
        for (int i=0;i<4;i++) {
            if (times[i]>=-0.000001f && times[i]<=earliest) {
                earliest=fmaxf(0,times[i]); event=i+2;
                normal=(Vec2){0,i==0?1:-1};
            }
        }
        game->ball.position.x+=v.x*remaining*earliest;
        game->ball.position.y+=v.y*remaining*earliest;
        if (event==4) return 2;
        if (event==5) return 1;
        if (event<0) break;
        if (event<2) {
            Rect box=old[event];
            float impact=used+remaining*earliest;
            box.x+=game->paddle_velocity[event].x*impact;
            box.y+=game->paddle_velocity[event].y*impact;
            paddle_bounce(game,box,game->paddle_velocity[event],normal);
        } else { game->ball.velocity.y=-game->ball.velocity.y; sound_play(SND_PONG_WALL); }
        game->ball.position.x+=normal.x*0.05f;
        game->ball.position.y+=normal.y*0.05f;
        remaining*=1-earliest;
    }
    return 0;
}

/** Move paddles within separate halves and advance the ball; dt is fixed seconds.
 * @note Normalizes diagonal input, measures actual clamped paddle velocity,
 *       and performs no GPIO, delay, allocation or display transfer.
 */
void pong_update(void *state, const InputSnapshot *snapshot, float seconds) {
    PongGame *game=state;
    if (game->over || seconds<=0) return;
    Rect old[2]={game->paddle[0],game->paddle[1]};
    for (unsigned int p=0;p<2;p++) {
        Vec2 axis=physics_limit((Vec2){snapshot->player[p].x,-snapshot->player[p].y},1);
        game->paddle[p].x=physics_clamp(old[p].x+axis.x*PONG_PADDLE_SPEED*seconds,p==0?12:164,p==0?148:300);
        game->paddle[p].y=physics_clamp(old[p].y+axis.y*PONG_PADDLE_SPEED*seconds,40,180);
        game->paddle_velocity[p]=(Vec2){(game->paddle[p].x-old[p].x)/seconds,
                                      (game->paddle[p].y-old[p].y)/seconds};
    }
    if (game->serve_seconds>0) { game->serve_seconds=fmaxf(0,game->serve_seconds-seconds); return; }
    if (game->ball.position.x<4) { point(game,1); return; }
    if (game->ball.position.x>316) { point(game,0); return; }
    unsigned int scorer=advance_ball(game,old,seconds);
    if (scorer) point(game,scorer-1);
}

/** Pong needs continuous axes only; button actions belong to the shared pause UI. */
static void input(void *state, const InputSnapshot *snapshot) { (void)state; (void)snapshot; }

/** Return whether the match reached the configured winning score. */
static bool finished(const void *state) { return ((const PongGame *)state)->over; }

/** Return the winning player index for the results colour. */
static int winner(const void *state) { return (int)((const PongGame *)state)->winner; }

/** Return a constant winner label for the common results screen. */
static const char *result(const void *state) { return ((const PongGame *)state)->winner==0?"PLAYER 1 WINS":"PLAYER 2 WINS"; }

const GameSpec pong_spec={"PONG","2 PLAYERS - TWO AXES",
    {"BOTH JOYSTICK AXES MOVE YOUR PADDLE.","STAY INSIDE YOUR OWN HALF.",
     "CENTRE HITS GO STRAIGHT.","EDGE HITS ANGLE UP OR DOWN.",
     "MOVEMENT INFLUENCES YOUR SHOT.","FIRST TO 7. CLICK K TO PAUSE.",NULL},
    pong_reset,input,pong_update,pong_draw,finished,result,PURPLE,pong_icon,winner,MUSIC_NONE,false};
