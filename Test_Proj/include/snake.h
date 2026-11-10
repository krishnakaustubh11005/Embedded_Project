/* Hardware-independent Snake rules and RAM state for the learning exercise. */
#ifndef SNAKE_H
#define SNAKE_H

#include <stdbool.h>
#include <stdint.h>

#define SNAKE_MAX_CELLS 1040

typedef enum { SNAKE_UP, SNAKE_DOWN, SNAKE_LEFT, SNAKE_RIGHT } SnakeDirection;
typedef enum { SNAKE_PLAYING, SNAKE_PAUSED, SNAKE_DEAD, SNAKE_WON } SnakeStatus;
typedef enum { SNAKE_NO_STEP, SNAKE_MOVED, SNAKE_ATE, SNAKE_DIED, SNAKE_WIN } SnakeEvent;
typedef struct { uint8_t x; uint8_t y; } SnakeCell;

typedef struct {
    SnakeCell body[SNAKE_MAX_CELLS];  // Head at index zero; tail at length - 1.
    bool occupied[SNAKE_MAX_CELLS];  // Constant-time collision and food checks.
    unsigned int columns, rows, length, score;
    SnakeCell food;
    SnakeDirection direction, pending_direction;
    SnakeStatus status;
    bool turn_queued, developer;
    uint32_t random_state;
} SnakeGame;

/** Reset RAM state for a valid grid; developer selects slower/wrapping rules. */
bool snake_reset(SnakeGame *game, unsigned int columns, unsigned int rows,
                 uint32_t seed, bool developer);
/** Queue at most one legal turn before the next movement step. */
bool snake_request_turn(SnakeGame *game, SnakeDirection direction);
/** Advance one grid cell, handling food/collisions; no hardware or delays. */
SnakeEvent snake_step(SnakeGame *game);
/** Pause/resume a live game; a dead/won game stays in its terminal state. */
void snake_pause(SnakeGame *game, bool paused);

#endif
