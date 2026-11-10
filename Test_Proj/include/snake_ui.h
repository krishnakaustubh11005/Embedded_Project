/* Snake screen composition; rendering changes RAM, not the panel directly. */
#ifndef SNAKE_UI_H
#define SNAKE_UI_H

#include "snake.h"

/** Draw the score, mode, status, snake and food into the existing framebuffer. */
void snake_draw(const SnakeGame *game);

#endif
