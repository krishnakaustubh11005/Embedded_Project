/* Snake adapter owns a movement timer around the validated portable rules. */
#ifndef CONSOLE_SNAKE_GAME_H
#define CONSOLE_SNAKE_GAME_H
#include "app/game.h"
#include "games/snake/snake.h"
typedef struct { SnakeGame game; float elapsed; } SnakeSession;
extern const GameSpec snake_spec;
/** Render rules state into SRAM; no SPI access. */
void snake_draw(const SnakeGame *game);
/** Draw the small launcher picture centred at cx,cy. */
void snake_icon(int cx, int cy);
/** Set explicit assistance, retaining score/body and restarting movement timing. */
void snake_session_assist(SnakeSession *session, bool enabled);
#endif
