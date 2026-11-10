/* Bounded command collector, independent of USB or Pico hardware. */
#ifndef CONSOLE_LINE_PARSER_H
#define CONSOLE_LINE_PARSER_H
#include <stdbool.h>
#include <stddef.h>
typedef struct { char text[48]; size_t length; bool overflow; } LineParser;
typedef enum { LINE_NONE, LINE_READY, LINE_TOO_LONG } LineStatus;
/** Feed one ASCII character; completed text remains valid until the next input. */
LineStatus line_parser_feed(LineParser *parser, char character);
#endif
