/* Shared console navigation and game storage; no GPIO/USB dependencies. */
#ifndef CONSOLE_APP_H
#define CONSOLE_APP_H
#include "app/game.h"
#include "diagnostics/metrics.h"
#include "games/snake/snake_game.h"
#include "games/pong/pong.h"
#include "games/soccer_pool/soccer.h"
#include "games/fighter/fighter.h"
typedef enum { SCREEN_BOOT, SCREEN_MENU, SCREEN_RULES, SCREEN_PLAY,
               SCREEN_PAUSE, SCREEN_RESULTS, SCREEN_DIAGNOSTICS } AppScreen;
typedef union { SnakeSession snake; PongGame pong; SoccerGame soccer; FighterGame fighter; } GameStorage;
typedef struct {
    AppScreen screen;
    unsigned int selected, pause_choice;   // selected==GAME_COUNT means diagnostics.
    uint32_t seed;
    float accumulator;
    bool reset_clock, snake_assist, recalibrate_requested;
    GameStorage game;
} App;
/** Initialize the console at its calibration screen using caller-owned RAM. */
void app_init(App *app, uint32_t seed);
/** Route each frame's new button events once, including global pause clicks. */
void app_input(App *app, const InputSnapshot *input);
/** Advance bounded fixed physics steps; returns number of skipped overdue ticks. */
unsigned int app_update(App *app, const InputSnapshot *input, uint64_t elapsed_us);
/** Compose the current screen into the SRAM framebuffer. */
void app_draw(const App *app, const InputSnapshot *input, const PerfReport *report);
/** Execute a complete exact USB command; return a constant response string. */
const char *app_command(App *app, const char *command);
#endif
