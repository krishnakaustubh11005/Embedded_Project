/* Small original bitmap letters and Snake graphics in the RGB565 framebuffer. */
#include "config.h"
#include "framebuffer.h"
#include "snake_ui.h"

/* Original 3x5 uppercase glyphs, one three-bit pattern per row, A through Z. */
static const uint8_t letters[26][5] = {
    {2,5,7,5,5}, {6,5,6,5,6}, {3,4,4,4,3}, {6,5,5,5,6},
    {7,4,6,4,7}, {7,4,6,4,4}, {3,4,5,5,3}, {5,5,7,5,5},
    {7,2,2,2,7}, {1,1,1,5,2}, {5,5,6,5,5}, {4,4,4,4,7},
    {5,7,7,5,5}, {5,7,7,7,5}, {2,5,5,5,2}, {6,5,6,4,4},
    {2,5,5,3,1}, {6,5,6,5,5}, {3,4,2,1,6}, {7,2,2,2,2},
    {5,5,5,5,7}, {5,5,5,5,2}, {5,5,7,7,5}, {5,5,2,5,5},
    {5,5,2,2,2}, {7,1,2,4,7}
};

/**
 * Draw a small uppercase label using six-by-ten pixel glyphs in RAM.
 * @param x Left pixel column; each character advances eight pixels.
 * @param y Top pixel row.
 * @param text Valid null-terminated label; spaces/unsupported characters are blank.
 * @param color RGB565 foreground colour; background pixels are preserved.
 * @note Clipping is performed by fb_fill_rect(); no SPI or allocation occurs.
 */
static void draw_label(int x, int y, const char *text, uint16_t color) {
    for (; *text; text++, x += 8) {
        if (*text < 'A' || *text > 'Z') continue;
        const uint8_t *glyph = letters[*text - 'A'];
        for (int row = 0; row < 5; row++) {
            for (int column = 0; column < 3; column++) {
                if (glyph[row] & (1u << (2 - column))) {
                    fb_fill_rect(x + column * 2, y + row * 2, 2, 2, color);
                }
            }
        }
    }
}

/**
 * Draw the food as a red ball inside its eight-pixel cell.
 * @param food Valid grid coordinate using the current Snake display layout.
 * @note Tests integer squared distance to avoid floating-point circle math.
 *       Changes framebuffer pixels only; the main loop sends them later.
 */
static void draw_food(SnakeCell food) {
    int centre_x = food.x * SNAKE_CELL_SIZE + SNAKE_CELL_SIZE / 2;
    int centre_y = SNAKE_HEADER_HEIGHT + food.y * SNAKE_CELL_SIZE + SNAKE_CELL_SIZE / 2;
    for (int y = -3; y <= 3; y++) {
        for (int x = -3; x <= 3; x++) {
            if (x * x + y * y <= 9) {
                fb_set_pixel(centre_x + x, centre_y + y, 0xF800);
            }
        }
    }
}

/**
 * Compose a complete Snake screen into the existing global RAM framebuffer.
 * @param game Valid read-only game state with dimensions matching config.h.
 * @note Shows four score digits, explicit DEV mode, play/pause/death/win status,
 *       control hints and ball-shaped food. Head is yellow, body green. Normal
 *       walls have a grey edge; DEV wrapping uses cyan. Does not send SPI data.
 */
void snake_draw(const SnakeGame *game) {
    fb_clear(FB_BLACK);
    draw_label(8, 4, "SCORE", FB_WHITE);
    unsigned int score = game->score;
    for (int digit = 3; digit >= 0; digit--) {
        fb_draw_digit(56 + digit * 16, 2, score % 10, FB_WHITE);
        score /= 10;
    }
    if (game->developer) draw_label(140, 4, "DEV", 0x07FF);
    const char *status = "PLAY";
    if (game->status == SNAKE_PAUSED) status = "PAUSED";
    if (game->status == SNAKE_DEAD) status = "GAME OVER";
    if (game->status == SNAKE_WON) status = "YOU WIN";
    draw_label(200, 4, status, FB_YELLOW);
    const char *hint = "A UP B RIGHT C DOWN D LEFT";
    if (game->status == SNAKE_PAUSED) hint = "USB RESUME TO CONTINUE";
    if (game->status == SNAKE_DEAD || game->status == SNAKE_WON) {
        hint = "ANY BUTTON RESTARTS";
    }
    draw_label(8, 28, hint, FB_WHITE);

    uint16_t edge = game->developer ? 0x07FF : 0x4208;
    fb_fill_rect(0, SNAKE_HEADER_HEIGHT, DISPLAY_WIDTH, 1, edge);
    fb_fill_rect(0, DISPLAY_HEIGHT - 1, DISPLAY_WIDTH, 1, edge);
    fb_fill_rect(0, SNAKE_HEADER_HEIGHT, 1, DISPLAY_HEIGHT - SNAKE_HEADER_HEIGHT, edge);
    fb_fill_rect(DISPLAY_WIDTH - 1, SNAKE_HEADER_HEIGHT, 1,
                 DISPLAY_HEIGHT - SNAKE_HEADER_HEIGHT, edge);
    if (game->status != SNAKE_WON) draw_food(game->food);
    for (unsigned int i = 0; i < game->length; i++) {
        fb_fill_rect(game->body[i].x * SNAKE_CELL_SIZE + 1,
            SNAKE_HEADER_HEIGHT + game->body[i].y * SNAKE_CELL_SIZE + 1,
            SNAKE_CELL_SIZE - 2, SNAKE_CELL_SIZE - 2, i == 0 ? FB_YELLOW : FB_GREEN);
    }
}
