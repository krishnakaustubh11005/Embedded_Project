/* Game registry adapter: player-one buttons, fixed cell timer and explicit DEV mode. */
#include <assert.h>
#include "config.h"
#include "graphics/gfx.h"
#include "audio/sound.h"
#include "games/snake/snake_game.h"

/** Reset a caller-owned SnakeSession using the screen grid and supplied food seed. */
static void reset(void *state, uint32_t seed) {
    SnakeSession *session=state;
    bool valid=snake_reset(&session->game,DISPLAY_WIDTH/SNAKE_CELL_SIZE,
        (DISPLAY_HEIGHT-SNAKE_HEADER_HEIGHT)/SNAKE_CELL_SIZE,seed,false);
    assert(valid); (void)valid;
    session->elapsed=0;
}

/** Queue player one's first new directional press, preserving classic reverse rejection. */
static void input(void *state, const InputSnapshot *snapshot) {
    SnakeSession *session=state;
    static const SnakeDirection direction[]={SNAKE_UP,SNAKE_RIGHT,SNAKE_DOWN,SNAKE_LEFT};
    for (unsigned int bit=0;bit<4;bit++) {
        if (snapshot->player[0].pressed & (1u<<bit)) {
            snake_request_turn(&session->game,direction[bit]);
            sound_play(SND_SNAKE_TURN);
            break;
        }
    }
}

/** Accumulate fixed physics seconds, advancing cells at eight Hz or four Hz in DEV. */
static void update(void *state, const InputSnapshot *snapshot, float seconds) {
    (void)snapshot;
    SnakeSession *session=state;
    session->elapsed+=seconds;
    float period=session->game.developer?0.25f:0.125f;
    while (session->elapsed>=period && session->game.status==SNAKE_PLAYING) {
        session->elapsed-=period;
        SnakeEvent event=snake_step(&session->game);
        if (event==SNAKE_ATE) sound_play(SND_SNAKE_EAT);
        else if (event==SNAKE_DIED) sound_play(SND_SNAKE_DIE);
    }
}

/** Render the session without changing game state or writing SPI. */
static void draw(const void *state) { snake_draw(&((const SnakeSession *)state)->game); }

/** Report terminal rule state; a pause in the application does not end the match. */
static bool finished(const void *state) {
    SnakeStatus status=((const SnakeSession *)state)->game.status;
    return status==SNAKE_DEAD || status==SNAKE_WON;
}

/** Return a constant result label suitable for the shared results screen. */
static const char *result(const void *state) {
    return ((const SnakeSession *)state)->game.status==SNAKE_WON?"BOARD COMPLETE":"GAME OVER";
}

/** Snake is single-player, so there is no winning player index. */
static int winner(const void *state) { (void)state; return -1; }

/** Enable/disable slower movement and wall wrapping in RAM; no flash persistence. */
void snake_session_assist(SnakeSession *session, bool enabled) {
    session->game.developer=enabled;
    session->elapsed=0;
}

const GameSpec snake_spec={"SNAKE","1 PLAYER - BUTTONS",
    {"EAT RED FOOD TO GROW AND SCORE.","A UP - B RIGHT - C DOWN - D LEFT.",
     "THE SNAKE KEEPS MOVING BY ITSELF.","NO IMMEDIATE REVERSE TURNS.",
     "WALL OR BODY CONTACT ENDS PLAY.","JOYSTICK CLICK K OPENS PAUSE.",NULL},
    reset,input,update,draw,finished,result,GREEN,snake_icon,winner,MUSIC_NONE,false};
