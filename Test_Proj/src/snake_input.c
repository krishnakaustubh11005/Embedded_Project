/* Timer debounce and bounded command parsing, independent of Pico hardware. */
#include <string.h>

#include "snake_input.h"

/**
 * Initialize button history so a button held at boot creates no false press.
 * @param buttons Writable debounce state for four buttons.
 * @param raw Bit i is one when button i is physically pressed.
 * @param now Current monotonic timestamp in microseconds.
 * @note Initializes RAM only. Caller converts active-low GPIO levels to bits.
 */
void snake_buttons_init(SnakeButtons *buttons, unsigned int raw, uint64_t now) {
    for (unsigned int i = 0; i < 4; i++) {
        buttons->candidate[i] = buttons->stable[i] = (raw & (1u << i)) != 0;
        buttons->changed_us[i] = now;
    }
}

/**
 * Accept a button state only after the observed candidate remains stable.
 * @param buttons Initialized debounce history, modified in place.
 * @param raw Pressed-bit mask sampled at now; bits 0..3 match buttons A..D.
 * @param now Monotonic microseconds; must not go backward between calls.
 * @param debounce_us Required observed stable period before accepting a change.
 * @return Bit mask of new stable presses; releases/held buttons return no event.
 * @note No sleeping or hardware I/O. Physical transitions between polls can be
 *       missed; the effective response includes polling gaps and debounce time.
 */
unsigned int snake_buttons_update(SnakeButtons *buttons, unsigned int raw,
                                 uint64_t now, unsigned int debounce_us) {
    unsigned int pressed = 0;
    for (unsigned int i = 0; i < 4; i++) {
        bool value = (raw & (1u << i)) != 0;
        if (value != buttons->candidate[i]) {
            buttons->candidate[i] = value;
            buttons->changed_us[i] = now;
        }
        if (value != buttons->stable[i] && now - buttons->changed_us[i] >= debounce_us) {
            buttons->stable[i] = value;
            if (value) pressed |= 1u << i;
        }
    }
    return pressed;
}

/**
 * Decode one completed command without touching game state or hardware.
 * @param line Null-terminated text; commands are case-sensitive and exact.
 * @return Recognized command ID, or CMD_UNKNOWN for unrecognized text.
 */
static SnakeCommand decode_command(const char *line) {
    if (strcmp(line, "DEV 2350") == 0) return CMD_DEV_ON;
    if (strcmp(line, "DEV OFF") == 0) return CMD_DEV_OFF;
    if (strcmp(line, "PAUSE") == 0) return CMD_PAUSE;
    if (strcmp(line, "RESUME") == 0) return CMD_RESUME;
    if (strcmp(line, "RESTART") == 0) return CMD_RESTART;
    if (strcmp(line, "HELP") == 0) return CMD_HELP;
    return CMD_UNKNOWN;
}

/**
 * Accumulate one character into a bounded serial line without blocking.
 * @param parser Zero-initialized or previously used parser state in RAM.
 * @param character Next available character; CR, LF and CRLF terminate lines.
 * @return CMD_NONE until a nonempty line ends, then its decoded command.
 * @note Handles backspace. Overlong lines are discarded through the next line
 *       ending and produce CMD_OVERFLOW, preventing a truncated secret command.
 *       Performs no serial reads, prints, sleeps or game-state changes.
 */
SnakeCommand snake_command_feed(SnakeParser *parser, char character) {
    if (character == '\r' || character == '\n') {
        SnakeCommand result = CMD_NONE;
        if (parser->overflow) result = CMD_OVERFLOW;
        else if (parser->length > 0) {
            parser->line[parser->length] = '\0';
            result = decode_command(parser->line);
        }
        parser->length = 0;
        parser->overflow = false;
        return result;
    }
    if (parser->overflow) return CMD_NONE;
    if (character == '\b' || character == 127) {
        if (parser->length > 0) parser->length--;
    } else if (character >= 32 && character <= 126) {
        if (parser->length + 1 < sizeof(parser->line)) {
            parser->line[parser->length++] = character;
        } else parser->overflow = true;
    }
    return CMD_NONE;
}
