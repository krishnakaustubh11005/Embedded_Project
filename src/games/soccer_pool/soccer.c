/* Turn-based disk football, with bounded substeps and no per-shot allocation. */
#include <math.h>
#include <string.h>
#include "config.h"
#include "graphics/gfx.h"
#include "games/soccer_pool/soccer.h"

/** Restore formation and clear motion while retaining match scores and active player. */
static void formation(SoccerGame *game) {
    static const Vec2 left[5]={{40,128},{78,86},{78,170},{115,104},{115,152}};
    for (unsigned int p=0;p<2;p++) {
        for (unsigned int i=0;i<5;i++) {
            Vec2 position={p?320-left[i].x:left[i].x,left[i].y};
            game->bodies[p*5+i]=(Body){position,{0,0},9,0.5f};
        }
    }
    game->bodies[10]=(Body){{160,128},{0,0},4,1};
    game->aim=(Vec2){game->turn==0?1:-1,0};
    game->selected=0; game->power=1; game->moving=false; game->rest_seconds=0;
}

/** Initialize a SoccerGame in caller-owned RAM, with player one taking kickoff. */
void soccer_reset(void *state, uint32_t seed) {
    (void)seed;
    SoccerGame *game=state;
    memset(game,0,sizeof(*game));
    formation(game);
}

/** Launch the selected team disk using aim and one of four quarter-power levels.
 * @return True for a new shot. Match completion or moving pieces reject repeats.
 */
bool soccer_shoot(SoccerGame *game) {
    if (game->moving || game->over || game->selected>=5 || game->turn>=2) return false;
    Vec2 direction=physics_unit(game->aim);
    if (physics_length(direction)<0.5f) direction=(Vec2){game->turn? -1:1,0};
    float speed=SOCCER_MAX_SHOT*(game->power+1)*0.25f;
    game->bodies[game->turn*5+game->selected].velocity=(Vec2){direction.x*speed,direction.y*speed};
    sound_play(SND_SOCCER_SHOT);
    game->moving=true; game->rest_seconds=0;
    return true;
}

/** Process active player's A/C selection, D power, B shoot and joystick aim.
 * @note Axes are right/up positive, so screen Y is inverted. Centred stick
 *       preserves the last aim. Input is frame-level, never repeated per physics tick.
 */
void soccer_input(void *state, const InputSnapshot *snapshot) {
    SoccerGame *game=state;
    if (game->moving || game->over) return;
    const Controller *controller=&snapshot->player[game->turn];
    Vec2 aim={controller->x,-controller->y};
    if (physics_length(aim)>0.15f) game->aim=physics_unit(aim);
    if (controller->pressed & (KEY_A|KEY_C)) sound_play(SND_SOCCER_SELECT);
    if (controller->pressed & KEY_D) sound_play(SND_SOCCER_POWER);
    if (controller->pressed & KEY_A) game->selected=(game->selected+4)%5;
    if (controller->pressed & KEY_C) game->selected=(game->selected+1)%5;
    if (controller->pressed & KEY_D) game->power=(game->power+1)%4;
    if (controller->pressed & KEY_B) (void)soccer_shoot(game);
}

/** Apply a speed cap and constant friction deceleration to one mutable body. */
static void friction(Body *body, float seconds) {
    body->velocity=physics_limit(body->velocity,SOCCER_MAX_SPEED);
    float speed=physics_length(body->velocity);
    float next=fmaxf(0,speed-SOCCER_FRICTION*seconds);
    float scale=speed>0?next/speed:0;
    body->velocity.x*=scale; body->velocity.y*=scale;
}

/** Award a fully crossed goal to the attacking team, then reset or finish the match.
 * @return True when a goal was found. Own goals credit the team opposite the net.
 */
static bool detect_goal(SoccerGame *game) {
    Body *ball=&game->bodies[10];
    if (ball->position.y-ball->radius<104 || ball->position.y+ball->radius>152) return false;
    int scorer=-1;
    if (ball->position.x+ball->radius<=8) scorer=1;
    if (ball->position.x-ball->radius>=312) scorer=0;
    if (scorer<0) return false;
    game->score[scorer]++;
    sound_play(SND_SOCCER_GOAL);
    game->turn=(unsigned int)scorer^1u;
    if (game->score[scorer]>=SOCCER_WIN_SCORE) { game->over=true; game->winner=(unsigned int)scorer; }
    formation(game);
    return true;
}

/** Keep all disks inside the pitch; football alone may pass through a goal mouth. */
static void boundary(Body *body, bool football) {
    float r=body->radius;
    if (body->position.y<36+r) {
        body->position.y=36+r; body->velocity.y=fabsf(body->velocity.y)*0.85f;
    }
    if (body->position.y>220-r) {
        body->position.y=220-r; body->velocity.y=-fabsf(body->velocity.y)*0.85f;
    }
    bool mouth=football && body->position.y-r>=104 && body->position.y+r<=152;
    if (!mouth && body->position.x<8+r) {
        body->position.x=8+r; body->velocity.x=fabsf(body->velocity.x)*0.85f;
    }
    if (!mouth && body->position.x>312-r) {
        body->position.x=312-r; body->velocity.x=-fabsf(body->velocity.x)*0.85f;
    }
}

/** Resolve the ball against four fixed round goalposts; disks use the full end walls. */
static void goalposts(SoccerGame *game) {
    static const Vec2 positions[]={{8,104},{8,152},{312,104},{312,152}};
    for (unsigned int i=0;i<4;i++) {
        Body post={positions[i],{0,0},2,0};
        physics_circle_pair(&game->bodies[10],&post,0.85f);
    }
}

/** Advance a moving shot using two bounded substeps per 120 Hz physics tick.
 * @note Four collision passes separate crowded disks. Speeds are capped before
 *       integration, limiting travel per substep. Turn changes only after all
 *       speeds remain under the stop threshold for 200 ms; pause is owned by app.
 */
void soccer_update(void *state, const InputSnapshot *snapshot, float seconds) {
    (void)snapshot;
    SoccerGame *game=state;
    if (!game->moving || game->over || seconds<=0) return;
    float step=seconds/2;
    for (unsigned int sub=0;sub<2;sub++) {
        for (unsigned int i=0;i<11;i++) {
            friction(&game->bodies[i],step);
            game->bodies[i].position.x+=game->bodies[i].velocity.x*step;
            game->bodies[i].position.y+=game->bodies[i].velocity.y*step;
        }
        if (detect_goal(game)) return;
        for (unsigned int pass=0;pass<4;pass++) {
            for (unsigned int a=0;a<11;a++) for (unsigned int b=a+1;b<11;b++) {
                Vec2 va=game->bodies[a].velocity, vb=game->bodies[b].velocity;
                float closing=physics_length((Vec2){va.x-vb.x,va.y-vb.y});
                /* Only real impacts clack; resting contacts stay silent. */
                if (physics_circle_pair(&game->bodies[a],&game->bodies[b],0.85f) && closing>25.0f)
                    sound_play(SND_SOCCER_CLACK);
            }
            goalposts(game);
            for (unsigned int i=0;i<11;i++) boundary(&game->bodies[i],i==10);
        }
        if (detect_goal(game)) return;
    }
    bool stopped=true;
    for (unsigned int i=0;i<11;i++) {
        if (physics_length(game->bodies[i].velocity)>=SOCCER_STOP_SPEED) stopped=false;
    }
    game->rest_seconds=stopped?game->rest_seconds+seconds:0;
    if (game->rest_seconds>=SOCCER_REST_SECONDS) {
        for (unsigned int i=0;i<11;i++) game->bodies[i].velocity=(Vec2){0,0};
        game->moving=false; game->turn^=1u; game->selected=0; game->power=1;
        sound_play(SND_SOCCER_TURN);
        game->aim=(Vec2){game->turn? -1:1,0};
    }
}

/** Report whether either team reached the match target. */
static bool finished(const void *state) { return ((const SoccerGame *)state)->over; }

/** Return the winning team index for the results colour. */
static int winner(const void *state) { return (int)((const SoccerGame *)state)->winner; }

/** Return the winning team's constant result label. */
static const char *result(const void *state) { return ((const SoccerGame *)state)->winner==0?"PLAYER 1 WINS":"PLAYER 2 WINS"; }

const GameSpec soccer_spec={"SOCCER POOL","2 PLAYERS - TAKE TURNS",
    {"FIVE DISKS EACH. FIRST TO 3 GOALS.","A/C SELECT A DISK. JOYSTICK AIMS.",
     "D CYCLES POWER. B SHOOTS.","WAIT UNTIL ALL PIECES STOP.",
     "TURNS ALTERNATE AFTER EACH SHOT.","OWN GOALS COUNT FOR YOUR OPPONENT.","CLICK K TO PAUSE."},
    soccer_reset,soccer_input,soccer_update,soccer_draw,finished,result,ORANGE,soccer_icon,winner,MUSIC_NONE,false};
