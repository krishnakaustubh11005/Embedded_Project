/* Pure button debounce and serial-line parsing; hardware I/O stays in main. */
#ifndef SNAKE_INPUT_H
#define SNAKE_INPUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    bool candidate[4], stable[4];
    uint64_t changed_us[4];
} SnakeButtons;

typedef enum {
    CMD_NONE, CMD_DEV_ON, CMD_DEV_OFF, CMD_PAUSE, CMD_RESUME,
    CMD_RESTART, CMD_HELP, CMD_UNKNOWN, CMD_OVERFLOW
} SnakeCommand;

typedef struct {
    char line[32];
    size_t length;
    bool overflow;
} SnakeParser;

/** Initialize four debounce states from a pressed-bit mask and timestamp. */
void snake_buttons_init(SnakeButtons *buttons, unsigned int raw, uint64_t now);
/** Return newly pressed stable bits; raw bit 1 means pressed, not GPIO high. */
unsigned int snake_buttons_update(SnakeButtons *buttons, unsigned int raw,
                                 uint64_t now, unsigned int debounce_us);
/** Feed one USB character; return a command only after a complete bounded line. */
SnakeCommand snake_command_feed(SnakeParser *parser, char character);

#endif
