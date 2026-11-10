/* Host checks for the real Snake rules, debounce and bounded USB parser. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "snake.h"
#include "snake_input.h"

static SnakeGame game;

/** Verify body uniqueness, occupancy count and food placement for valid state. */
static void check_invariants(void) {
    unsigned int count = 0;
    bool seen[SNAKE_MAX_CELLS] = {false};
    for (unsigned int i = 0; i < game.length; i++) {
        assert(game.body[i].x < game.columns && game.body[i].y < game.rows);
        unsigned int index = game.body[i].y * game.columns + game.body[i].x;
        assert(!seen[index] && game.occupied[index]);
        seen[index] = true;
    }
    for (unsigned int i = 0; i < game.columns * game.rows; i++) {
        count += game.occupied[i];
        assert(game.occupied[i] == seen[i]);
    }
    assert(count == game.length);
    if (game.status != SNAKE_WON) {
        assert(game.food.x < game.columns && game.food.y < game.rows);
        assert(!game.occupied[game.food.y * game.columns + game.food.x]);
    }
}

/** Check reset validation, legal turns, queued-turn limits, growth and pause. */
static void test_movement(void) {
    assert(snake_reset(&game, 8, 6, 1, false));
    assert(game.length == 3 && game.score == 0);
    assert(!snake_reset(&game, 3, 6, 2, false));
    assert(!snake_reset(&game, 255, 255, 2, false));
    assert(game.columns == 8 && game.length == 3);
    assert(!snake_request_turn(&game, SNAKE_LEFT));
    assert(!snake_request_turn(&game, SNAKE_RIGHT));
    assert(!snake_request_turn(&game, (SnakeDirection)99));
    assert(snake_request_turn(&game, SNAKE_UP));
    assert(!snake_request_turn(&game, SNAKE_LEFT)); // Already queued for this tick.
    game.food = (SnakeCell){0, 0};
    assert(snake_step(&game) == SNAKE_MOVED);
    assert(game.body[0].x == 4 && game.body[0].y == 2);
    assert(game.direction == SNAKE_UP);
    assert(snake_request_turn(&game, SNAKE_LEFT));
    game.food = (SnakeCell){3, 2};
    assert(snake_step(&game) == SNAKE_ATE);
    assert(game.length == 4 && game.score == 1);
    check_invariants();
    SnakeCell head = game.body[0];
    snake_pause(&game, true);
    assert(snake_step(&game) == SNAKE_NO_STEP);
    assert(!snake_request_turn(&game, SNAKE_DOWN));
    assert(game.body[0].x == head.x && game.body[0].y == head.y);
    snake_pause(&game, false);
    assert(game.status == SNAKE_PLAYING);
}

/** Set up a compact bent snake to distinguish body collision from vacating tail. */
static void bent_snake(bool extra_tail) {
    assert(snake_reset(&game, 8, 6, 3, false));
    memset(game.occupied, 0, sizeof(game.occupied));
    const SnakeCell cells[] = {{2,2}, {2,3}, {3,3}, {3,2}, {4,2}};
    game.length = extra_tail ? 5 : 4;
    memcpy(game.body, cells, game.length * sizeof(SnakeCell));
    for (unsigned int i = 0; i < game.length; i++) {
        game.occupied[game.body[i].y * game.columns + game.body[i].x] = true;
    }
    game.direction = SNAKE_UP;
    game.food = (SnakeCell){0,0};
    assert(snake_request_turn(&game, SNAKE_RIGHT));
}

/** Check normal wall death, developer wrapping, self collision and tail movement. */
static void test_collisions(void) {
    assert(snake_reset(&game, 8, 6, 4, false));
    game.food = (SnakeCell){0,0};
    while (game.status == SNAKE_PLAYING) snake_step(&game);
    assert(game.status == SNAKE_DEAD && game.body[0].x == 7);
    snake_pause(&game, false);
    assert(game.status == SNAKE_DEAD);

    assert(snake_reset(&game, 8, 6, 4, true));
    game.food = (SnakeCell){0,0};
    for (int i = 0; i < 4; i++) assert(snake_step(&game) == SNAKE_MOVED);
    assert(game.body[0].x == 0 && game.status == SNAKE_PLAYING);
    check_invariants();
    bent_snake(true);
    assert(snake_step(&game) == SNAKE_DIED);
    bent_snake(false);
    assert(snake_step(&game) == SNAKE_MOVED);
    check_invariants();
}

/** Exercise the final growth at maximum array capacity without an endless spawn. */
static void test_full_board(void) {
    memset(&game, 0, sizeof(game));
    game.columns = 40;
    game.rows = 26;
    game.length = SNAKE_MAX_CELLS - 1;
    game.status = SNAKE_PLAYING;
    game.direction = SNAKE_LEFT;
    game.food = (SnakeCell){0,0};
    // A continuous serpentine body occupies every cell except the food at (0,0).
    for (unsigned int i = 1; i < SNAKE_MAX_CELLS; i++) {
        unsigned int y = i / 40;
        unsigned int x = y % 2 ? 39 - i % 40 : i % 40;
        game.body[i - 1] = (SnakeCell){x, y};
        game.occupied[y * 40 + x] = true;
    }
    check_invariants();
    assert(snake_step(&game) == SNAKE_WIN);
    assert(game.status == SNAKE_WON && game.length == SNAKE_MAX_CELLS);
    check_invariants();
}

/** Check free food placement and occupancy consistency through many valid games. */
static void test_food_and_long_run(void) {
    for (uint32_t seed = 0; seed < 1000; seed++) {
        assert(snake_reset(&game, 40, 26, seed, false));
        check_invariants();
    }
    uint32_t random = 19;
    for (unsigned int i = 0; i < 20000; i++) {
        if (i == 0 || game.status != SNAKE_PLAYING) {
            assert(snake_reset(&game, 12, 8, i + 1, true));
        }
        random = random * 1664525u + 1013904223u;
        snake_request_turn(&game, (SnakeDirection)(random >> 30));
        snake_step(&game);
        check_invariants();
    }
}

/** Check observed stable time, bounce rejection, held keys, release and boot state. */
static void test_buttons(void) {
    SnakeButtons buttons;
    snake_buttons_init(&buttons, 0, 0);
    assert(snake_buttons_update(&buttons, 1, 1000, 20000) == 0);
    assert(snake_buttons_update(&buttons, 0, 5000, 20000) == 0);
    assert(snake_buttons_update(&buttons, 1, 10000, 20000) == 0);
    assert(snake_buttons_update(&buttons, 1, 29999, 20000) == 0);
    assert(snake_buttons_update(&buttons, 1, 30000, 20000) == 1);
    assert(snake_buttons_update(&buttons, 1, 1000000, 20000) == 0);
    assert(snake_buttons_update(&buttons, 0, 1000100, 20000) == 0);
    assert(snake_buttons_update(&buttons, 0, 1020100, 20000) == 0);
    assert(snake_buttons_update(&buttons, 10, 1030000, 20000) == 0);
    assert(snake_buttons_update(&buttons, 10, 1050000, 20000) == 10);
    snake_buttons_init(&buttons, 1, 0);
    assert(snake_buttons_update(&buttons, 1, 100000, 20000) == 0);
}

/** Feed a complete string and return its last nonempty parser event. */
static SnakeCommand feed(SnakeParser *parser, const char *text) {
    SnakeCommand result = CMD_NONE;
    for (; *text; text++) {
        SnakeCommand event = snake_command_feed(parser, *text);
        if (event != CMD_NONE) result = event;
    }
    return result;
}

/** Check exact commands, partial lines, CR/LF, backspace and overflow recovery. */
static void test_commands(void) {
    SnakeParser parser = {0};
    assert(feed(&parser, "DEV ") == CMD_NONE);
    assert(feed(&parser, "2350\r") == CMD_DEV_ON);
    assert(feed(&parser, "\n") == CMD_NONE);
    assert(feed(&parser, "DEV OFF\n") == CMD_DEV_OFF);
    assert(feed(&parser, "PAUSE\n") == CMD_PAUSE);
    assert(feed(&parser, "RESUME\r\n") == CMD_RESUME);
    assert(feed(&parser, "RESTART\n") == CMD_RESTART);
    assert(feed(&parser, "HELP\n") == CMD_HELP);
    assert(feed(&parser, "DEV 2351\b0\n") == CMD_DEV_ON);
    assert(feed(&parser, "DEV2350\n") == CMD_UNKNOWN);
    for (int i = 0; i < 40; i++) assert(snake_command_feed(&parser, 'A') == CMD_NONE);
    assert(feed(&parser, "\n") == CMD_OVERFLOW);
    assert(feed(&parser, "RESTART\n") == CMD_RESTART);
    assert(feed(&parser, "\n\r\n") == CMD_NONE);
}

/** Run host-only assertions; success verifies rules, not real GPIO or display timing. */
int main(void) {
    test_movement();
    test_collisions();
    test_full_board();
    test_food_and_long_run();
    test_buttons();
    test_commands();
    puts("Snake rule, debounce and command parser checks passed.");
    return 0;
}
