/* Two-axis Pong model; all positions are pixels, velocities pixels/second. */
#ifndef CONSOLE_PONG_H
#define CONSOLE_PONG_H
#include "app/game.h"
#include "games/physics.h"
typedef struct {
    Rect paddle[2];
    Vec2 paddle_velocity[2];
    Body ball;
    unsigned int score[2], winner, hits;
    float serve_seconds;
    bool over;
} PongGame;
extern const GameSpec pong_spec;
/** Initialize a caller-owned PongGame and a timed serve. */
void pong_reset(void *state, uint32_t seed);
/** Update both paddles and sweep the ball for dt seconds; caller uses fixed small steps. */
void pong_update(void *state, const InputSnapshot *snapshot, float seconds);
/** Draw the match into RAM, including score and own-half movement markers. */
void pong_draw(const void *state);
/** Draw the small launcher picture centred at cx,cy. */
void pong_icon(int cx, int cy);
#endif
