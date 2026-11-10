/* Button Snake, nonblocking USB developer commands, and measured frame budget. */
#include <stdio.h>
#include <string.h>

#include "hardware/clocks.h"
#include "hardware/regs/clocks.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"
#include "config.h"
#include "framebuffer.h"
#include "snake.h"
#include "snake_input.h"
#include "snake_ui.h"
#include "st7789.h"

_Static_assert(DISPLAY_WIDTH % SNAKE_CELL_SIZE == 0, "Grid must fit display width");
_Static_assert((DISPLAY_HEIGHT - SNAKE_HEADER_HEIGHT) % SNAKE_CELL_SIZE == 0,
               "Grid must fit below the header");

/* Static storage keeps game arrays and the framebuffer off the small stack. */
static SnakeGame game;
static SnakeButtons buttons;
static SnakeParser parser;
static const unsigned int button_pins[4] = {
    BUTTON_A_PIN, BUTTON_B_PIN, BUTTON_C_PIN, BUTTON_D_PIN
};
/* Button order is independent of the direction enum's numeric values. */
static const SnakeDirection button_directions[4] = {
    SNAKE_UP, SNAKE_RIGHT, SNAKE_DOWN, SNAKE_LEFT
};

typedef struct {
    uint64_t window_start, last_sample;
    uint64_t input, logic, draw, transfer;
    uint64_t max_controls, max_frame, max_gap;
    unsigned int frames, over_budget, gap_over_budget, skipped_ticks;
} SnakeMetrics;

/**
 * Read four active-low GPIOs and convert them into a pressed-bit mask.
 * @return Bits 0..3 represent A/up, B/right, C/down, D/left; one means pressed.
 * @note Requires GPIO inputs with pull-ups initialized; raw sampling only.
 */
static unsigned int read_buttons(void) {
    unsigned int mask = 0;
    for (unsigned int i = 0; i < 4; i++) {
        if (!gpio_get(button_pins[i])) mask |= 1u << i;
    }
    return mask;
}

/**
 * Return the movement period for normal or assisted play.
 * @return Microseconds per grid step: 125000 normal, 250000 developer.
 * @note Reads game mode only; does not sleep or depend on rendering FPS.
 */
static unsigned int movement_period(void) {
    return game.developer ? SNAKE_DEV_MOVE_US : SNAKE_NORMAL_MOVE_US;
}

/**
 * Restart the game on the display-sized grid while keeping the selected mode.
 * @param now Current microsecond timestamp, also used as a varying food seed.
 * @note Changes game RAM only. Stops with an error if the layout is invalid.
 */
static void restart_game(uint64_t now) {
    bool developer = game.developer;
    if (!snake_reset(&game, DISPLAY_WIDTH / SNAKE_CELL_SIZE,
        (DISPLAY_HEIGHT - SNAKE_HEADER_HEIGHT) / SNAKE_CELL_SIZE,
        (uint32_t)now, developer)) {
        panic("Snake grid configuration is invalid");
    }
}

/**
 * Apply one completed USB command and print its response.
 * @param command Result of the bounded line parser.
 * @param now Current monotonic microseconds for restart seeding.
 * @return True when the movement deadline should restart from now.
 * @note DEV is an intentional assist flag, not identity detection. Prints can
 *       add control time; timing metrics include these command responses.
 */
static bool execute_command(SnakeCommand command, uint64_t now) {
    switch (command) {
        case CMD_DEV_ON:
            game.developer = true;
            printf("Developer mode ON: slower movement and wall wrapping\n");
            return true;
        case CMD_DEV_OFF:
            game.developer = false;
            printf("Developer mode OFF: normal speed and walls\n");
            return true;
        case CMD_PAUSE:
            snake_pause(&game, true);
            printf("Pause requested\n");
            return true;
        case CMD_RESUME:
            snake_pause(&game, false);
            printf("Resume requested\n");
            return true;
        case CMD_RESTART:
            restart_game(now);
            printf("Snake restarted\n");
            return true;
        case CMD_HELP:
            printf("Commands: DEV 2350 | DEV OFF | PAUSE | RESUME | RESTART | HELP\n");
            break;
        case CMD_UNKNOWN: printf("Unknown command; type HELP\n"); break;
        case CMD_OVERFLOW: printf("Command discarded: line too long\n"); break;
        case CMD_NONE: break;
    }
    return false;
}

/**
 * Read a bounded number of available USB characters without waiting for input.
 * @param now Current microseconds passed to command actions.
 * @return True if any command changed timing-sensitive state.
 * @note Partial lines persist across frames; getchar_timeout_us(0) returns
 *       immediately when there is no character. Serial work is bounded per loop.
 */
static bool poll_serial(uint64_t now) {
    bool reschedule = false;
    for (unsigned int i = 0; i < SNAKE_SERIAL_CHARS_PER_FRAME; i++) {
        int character = getchar_timeout_us(0);
        if (character == PICO_ERROR_TIMEOUT) break;
        if (execute_command(snake_command_feed(&parser, (char)character), now)) {
            reschedule = true;
        }
    }
    return reschedule;
}

/**
 * Route new debounced presses to one turn or a game-over restart.
 * @param pressed Event mask, not raw/held button levels.
 * @param now Timestamp used if restart is needed.
 * @return True after a restart, requesting a new movement deadline.
 * @note Simultaneous presses have A/B/C/D priority. At most one turn is queued
 *       per movement step; opposite turns and turns while paused are ignored.
 */
static bool handle_buttons(unsigned int pressed, uint64_t now) {
    if (!pressed) return false;
    if (game.status == SNAKE_DEAD || game.status == SNAKE_WON) {
        restart_game(now);
        return true;
    }
    for (unsigned int i = 0; i < 4; i++) {
        if (pressed & (1u << i)) {
            snake_request_turn(&game, button_directions[i]);
            break;
        }
    }
    return false;
}

/**
 * Execute due fixed-timestep moves with a bounded catch-up limit.
 * @param now Monotonic microseconds at the start of this update.
 * @param deadline Next scheduled move; updated in place.
 * @return Count of overdue ticks deliberately dropped after a long stall.
 * @note Paused/dead/won games continually renew their deadline, avoiding a burst
 *       of old moves on resume. Does no drawing, sleeping or hardware access.
 */
static unsigned int update_game(uint64_t now, uint64_t *deadline) {
    unsigned int period = movement_period();
    if (game.status != SNAKE_PLAYING) {
        *deadline = now + period;
        return 0;
    }
    for (unsigned int i = 0; now >= *deadline && i < SNAKE_MAX_CATCHUP_STEPS; i++) {
        snake_step(&game);
        *deadline += period;
        if (game.status != SNAKE_PLAYING) break;
    }
    if (now >= *deadline) {
        unsigned int dropped = (unsigned int)((now - *deadline) / period + 1);
        *deadline = now + period;
        return dropped;
    }
    return 0;
}

/**
 * Report observed frame timing approximately once per second and reset totals.
 * @param stats Accumulated RAM metrics; frames must be nonzero when reporting.
 * @param now Timestamp after the most recent completed SPI transfer.
 * @note FPS includes prior reporting time. Stage averages exclude the periodic
 *       report itself; max input gap includes it. A gap can exceed 1/43 second
 *       even when CPU input/logic work is short, especially with blocking SPI.
 */
static void report_metrics(SnakeMetrics *stats, uint64_t now) {
    uint64_t elapsed = now - stats->window_start;
    if (elapsed < 1000000 || stats->frames == 0) return;
    printf("Snake: %.2f FPS | input=%u logic=%u draw=%u SPI=%u us/frame | "
           "controls_max=%u frame_max=%u input_gap_max=%u us | "
           "over_budget=%u gap_over=%u dropped_ticks=%u | SPI clock=%u Hz | DEV=%u\n",
           stats->frames * 1000000.0 / elapsed,
           (unsigned int)(stats->input / stats->frames),
           (unsigned int)(stats->logic / stats->frames),
           (unsigned int)(stats->draw / stats->frames),
           (unsigned int)(stats->transfer / stats->frames),
           (unsigned int)stats->max_controls, (unsigned int)stats->max_frame,
           (unsigned int)stats->max_gap, stats->over_budget, stats->gap_over_budget,
           stats->skipped_ticks, (unsigned int)spi_get_baudrate(DISPLAY_SPI), game.developer);
    uint64_t last_sample = stats->last_sample;
    memset(stats, 0, sizeof(*stats));
    stats->last_sample = last_sample;
    stats->window_start = now;  // Include this report's cost in the next FPS interval.
}

/**
 * Run button Snake and measure controls plus full-frame transfer against 43 FPS.
 * @return Never returns during normal operation.
 * @note Sets CPU/peripheral to 125 MHz and uses the tested 62.5 MHz SPI request.
 *       Polls/debounces four shield buttons once per frame and parses bounded
 *       USB lines. The snake moves on its own timer, not once per render.
 *       One core and blocking SPI mean sample gaps still include transfer time.
 *       GPIO/button response is not guaranteed to fit one frame: debounce and
 *       movement deadlines intentionally add delay, which is distinct from
 *       execution time. No flash persistence, DMA, joystick ADC or heap is used.
 */
int main(void) {
    set_sys_clock_khz(SNAKE_CPU_KHZ, true);
    clock_configure_undivided(clk_peri, 0,
        CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS, clock_get_hz(clk_sys));
    stdio_init_all();
    st7789_init();
    for (unsigned int i = 0; i < 4; i++) {
        gpio_init(button_pins[i]);
        gpio_set_dir(button_pins[i], GPIO_IN);
        gpio_pull_up(button_pins[i]);
    }
    uint64_t now = time_us_64();
    snake_buttons_init(&buttons, read_buttons(), now);
    restart_game(now);
    uint64_t next_move = now + movement_period();
    SnakeMetrics stats = {.window_start = now};
    printf("Snake: CPU=%u peri=%u SPI=%u Hz; A=up B=right C=down D=left; type HELP\n",
        (unsigned int)clock_get_hz(clk_sys), (unsigned int)clock_get_hz(clk_peri),
        (unsigned int)spi_get_baudrate(DISPLAY_SPI));

    while (true) {
        uint64_t start = time_us_64();
        if (stats.last_sample) {
            uint64_t gap = start - stats.last_sample;
            if (gap > stats.max_gap) stats.max_gap = gap;
            if (gap > SNAKE_FRAME_BUDGET_US) stats.gap_over_budget++;
        }
        stats.last_sample = start;
        unsigned int pressed = snake_buttons_update(&buttons, read_buttons(), start,
                                                    SNAKE_DEBOUNCE_US);
        bool reschedule = poll_serial(start);
        uint64_t input_end = time_us_64();
        if (handle_buttons(pressed, start)) reschedule = true;
        if (reschedule) next_move = start + movement_period();
        stats.skipped_ticks += update_game(input_end, &next_move);
        uint64_t logic_end = time_us_64();
        snake_draw(&game);
        uint64_t draw_end = time_us_64();
        st7789_push_frame(fb_pixels());
        uint64_t end = time_us_64();

        stats.frames++;
        stats.input += input_end - start;
        stats.logic += logic_end - input_end;
        stats.draw += draw_end - logic_end;
        stats.transfer += end - draw_end;
        if (logic_end - start > stats.max_controls) stats.max_controls = logic_end - start;
        if (end - start > stats.max_frame) stats.max_frame = end - start;
        if (end - start > SNAKE_FRAME_BUDGET_US) stats.over_budget++;
        report_metrics(&stats, end);
    }
}
