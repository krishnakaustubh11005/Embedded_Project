/* Five-disk teams and one football; no hardware dependencies. */
#ifndef CONSOLE_SOCCER_H
#define CONSOLE_SOCCER_H
#include "app/game.h"
#include "games/physics.h"
typedef struct {
    Body bodies[11];             // Teams 0..4, 5..9; football at 10.
    unsigned int score[2], turn, selected, power, winner;
    Vec2 aim;
    float rest_seconds;
    bool moving, over;
} SoccerGame;
extern const GameSpec soccer_spec;
/** Reset scores and formation; seed determines the first player. */
void soccer_reset(void *state, uint32_t seed);
/** Read only the active controller while aiming; ignore gameplay input during motion. */
void soccer_input(void *state, const InputSnapshot *snapshot);
/** Launch the selected disk once; returns false if a shot is already in progress. */
bool soccer_shoot(SoccerGame *game);
/** Integrate fixed physics seconds with friction, circles, goalposts and turn completion. */
void soccer_update(void *state, const InputSnapshot *snapshot, float seconds);
/** Compose the field and aiming aids into SRAM without modifying match state. */
void soccer_draw(const void *state);
/** Draw the small launcher picture centred at cx,cy. */
void soccer_icon(int cx, int cy);
#endif
