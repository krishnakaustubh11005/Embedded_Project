/* Pure Snake rules: no GPIO, display writes, serial I/O or wall-clock waits. */
#include <string.h>

#include "games/snake/snake.h"

/**
 * Choose food among currently empty cells using a small PRNG.
 * @param game Valid initialized game with an up-to-date occupancy array.
 * @return False when the board is full; otherwise food is updated in RAM.
 * @note Visits the grid once, so placement cannot loop forever near a win.
 *       Modulo selection has slight statistical bias, acceptable for this game.
 */
static bool place_food(SnakeGame *game) {
    unsigned int free_count = game->columns * game->rows - game->length;
    if (free_count == 0) {
        return false;
    }
    // Unsigned overflow is defined and supplies the PRNG's wraparound.
    game->random_state = game->random_state * 1664525u + 1013904223u;
    unsigned int choice = game->random_state % free_count;
    for (unsigned int i = 0; i < game->columns * game->rows; i++) {
        if (!game->occupied[i]) {
            if (choice == 0) {
                game->food = (SnakeCell){i % game->columns, i / game->columns};
                return true;
            }
            choice--;
        }
    }
    return false;  // Only reachable if the occupancy/length invariant was broken.
}

/**
 * Initialize a three-cell snake heading right, with food on a free grid cell.
 * @param game Valid writable RAM state; no previous initialization is needed.
 * @param columns Grid width, 4..255 cells.
 * @param rows Grid height, 2..255; columns*rows must fit SNAKE_MAX_CELLS.
 * @param seed Initial PRNG state; determines the food sequence.
 * @param developer Enable wall wrapping; movement timing is chosen by the caller.
 * @return False for invalid dimensions, leaving the previous state untouched.
 * @note Resets score and pause/death state. No hardware, allocation or flash I/O.
 */
bool snake_reset(SnakeGame *game, unsigned int columns, unsigned int rows,
                 uint32_t seed, bool developer) {
    if (columns < 4 || columns > 255 || rows < 2 || rows > 255 ||
        columns * rows > SNAKE_MAX_CELLS) {
        return false;
    }
    memset(game, 0, sizeof(*game));
    game->columns = columns;
    game->rows = rows;
    game->length = 3;
    game->direction = game->pending_direction = SNAKE_RIGHT;
    game->status = SNAKE_PLAYING;
    game->developer = developer;
    game->random_state = seed;
    for (unsigned int i = 0; i < game->length; i++) {
        game->body[i] = (SnakeCell){columns / 2 - i, rows / 2};
        game->occupied[game->body[i].y * columns + game->body[i].x] = true;
    }
    return place_food(game);
}

/**
 * Queue a legal direction change relative to the actual movement heading.
 * @param game Valid initialized state; only pending turn fields are modified.
 * @param direction One of UP, DOWN, LEFT or RIGHT.
 * @return True if accepted; false for opposite/same direction, invalid values,
 *         non-playing state, or another turn already queued for this step.
 * @note Prevents two rapid button events from reversing before a movement tick.
 */
bool snake_request_turn(SnakeGame *game, SnakeDirection direction) {
    if (game->status != SNAKE_PLAYING || game->turn_queued ||
        (unsigned int)direction > SNAKE_RIGHT || direction == game->direction ||
        (unsigned int)direction == ((unsigned int)game->direction ^ 1u)) {
        return false;
    }
    game->pending_direction = direction;
    game->turn_queued = true;
    return true;
}

/**
 * Advance the snake by one grid cell and update occupancy, score and food.
 * @param game Valid initialized state owned by the calling loop.
 * @return Event describing movement, eating, death, win, or no step while paused.
 * @note Normal walls/body hits end play. Developer mode wraps walls, but body
 *       hits still end play. Moving into the vacating tail cell is legal.
 *       The caller controls tick timing; this function never sleeps or draws.
 */
SnakeEvent snake_step(SnakeGame *game) {
    if (game->status != SNAKE_PLAYING) {
        return SNAKE_NO_STEP;
    }
    if (game->turn_queued) {
        game->direction = game->pending_direction;
        game->turn_queued = false;
    }
    int x = game->body[0].x;
    int y = game->body[0].y;
    if (game->direction == SNAKE_UP) y--;
    if (game->direction == SNAKE_DOWN) y++;
    if (game->direction == SNAKE_LEFT) x--;
    if (game->direction == SNAKE_RIGHT) x++;

    if (game->developer) {
        x = (x + (int)game->columns) % (int)game->columns;
        y = (y + (int)game->rows) % (int)game->rows;
    } else if (x < 0 || y < 0 || x >= (int)game->columns || y >= (int)game->rows) {
        game->status = SNAKE_DEAD;
        return SNAKE_DIED;
    }
    bool grows = x == game->food.x && y == game->food.y;
    SnakeCell tail = game->body[game->length - 1];
    if (game->occupied[y * game->columns + x] &&
        (grows || x != tail.x || y != tail.y)) {
        game->status = SNAKE_DEAD;
        return SNAKE_DIED;
    }
    if (grows) {
        game->length++;
        game->score++;
    } else {
        game->occupied[tail.y * game->columns + tail.x] = false;
    }
    // memmove supports overlap while shifting the old head/body toward the tail.
    memmove(game->body + 1, game->body, (game->length - 1) * sizeof(SnakeCell));
    game->body[0] = (SnakeCell){x, y};
    game->occupied[y * game->columns + x] = true;
    if (grows && !place_food(game)) {
        game->status = SNAKE_WON;
        return SNAKE_WIN;
    }
    return grows ? SNAKE_ATE : SNAKE_MOVED;
}

/**
 * Pause or resume movement without changing the snake, score or pending turn.
 * @param game Valid initialized RAM state.
 * @param paused True requests pause, false requests resume.
 * @note Terminal dead/won states are preserved; restart requires snake_reset().
 */
void snake_pause(SnakeGame *game, bool paused) {
    if (game->status == SNAKE_PLAYING || game->status == SNAKE_PAUSED) {
        game->status = paused ? SNAKE_PAUSED : SNAKE_PLAYING;
    }
}
