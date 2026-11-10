/* Nonblocking command assembly: no I/O, allocation or game-state access. */
#include "platform/line_parser.h"

/** Feed an initially zeroed parser; CR/LF terminate lines and backspace edits.
 * @return READY for a nonempty line, TOO_LONG after discarding overflow, or NONE.
 */
LineStatus line_parser_feed(LineParser *parser, char character) {
    if (character == '\r' || character == '\n') {
        LineStatus status = parser->overflow ? LINE_TOO_LONG : parser->length ? LINE_READY : LINE_NONE;
        parser->text[parser->length] = 0;
        parser->length = 0;
        parser->overflow = false;
        return status;
    }
    if (parser->overflow) return LINE_NONE;
    if (character == '\b' || character == 127) {
        if (parser->length) parser->length--;
    } else if (character >= 32 && character <= 126) {
        if (parser->length + 1 < sizeof(parser->text)) parser->text[parser->length++] = character;
        else parser->overflow = true;
    }
    return LINE_NONE;
}
