/* Phase 3: render one complete RAM image, send it, then measure completed FPS. */
#include <stdio.h>

#include "hardware/spi.h"
#include "pico/stdlib.h"
#include "config.h"
#include "framebuffer.h"
#include "st7789.h"

#ifdef FPS_BENCHMARK
#include "hardware/clocks.h"
#include "hardware/regs/clocks.h"
#endif

/* Group the moving rectangle's state instead of using unrelated variables. */
typedef struct {
    int x;
    int y;
    int dx;
    int dy;
} Rectangle;

/* Original 8x8 smile sprite, stored as eight bit patterns in flash. */
static const uint8_t smile[8] = {0x3C, 0x42, 0xA5, 0x81, 0xA5, 0x99, 0x42, 0x3C};

#ifdef FPS_BENCHMARK
static const unsigned int benchmark_clocks[] = FPS_BENCHMARK_CLOCKS_HZ;
static unsigned int benchmark_boot_cpu_khz;

/**
 * Select one benchmark stage and report its actual hardware clocks.
 * @param stage Zero-based index into benchmark_clocks; must be in range.
 * @note Call only between frames, after SPI has finished and display CS is high.
 *       Stops SPI before changing clocks. The final stage uses a 125 MHz CPU;
 *       other stages restore the saved boot CPU clock. Reconnects clk_peri to
 *       clk_sys and restarts SPI in 8-bit mode 0. Prints requested/actual rates.
 *       The SDK stops the program if a required system clock is unattainable.
 */
static void select_benchmark_clock(unsigned int stage) {
    const unsigned int stage_count = sizeof(benchmark_clocks) / sizeof(benchmark_clocks[0]);
    unsigned int cpu_khz = stage == stage_count - 1
        ? FPS_BENCHMARK_ALT_CPU_KHZ : benchmark_boot_cpu_khz;

    // Called between completed frames with display CS high. Stop SPI before
    // changing its source, then restore 8-bit mode-0 command operation.
    spi_deinit(DISPLAY_SPI);
    if (clock_get_hz(clk_sys) != cpu_khz * 1000u) {
        set_sys_clock_khz(cpu_khz, true);
    }
    // The SDK may move clk_peri to 48 MHz during a system-clock change.
    // Explicitly select clk_sys so the SPI input is the intended frequency.
    clock_configure_undivided(clk_peri, 0,
        CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS, clock_get_hz(clk_sys));
    unsigned int actual = spi_init(DISPLAY_SPI, benchmark_clocks[stage]);
    printf("BENCH stage %u: request=%u Hz, actual=%u Hz, CPU=%u Hz, peri=%u Hz, 16-bit transfer%s\n",
           stage + 1, benchmark_clocks[stage], actual,
           (unsigned int)clock_get_hz(clk_sys), (unsigned int)clock_get_hz(clk_peri),
           actual > 15000000 ? " (higher-clock experiment)" : "");
}
#endif

/**
 * Advance the rectangle one animation step and bounce at its bounds.
 * @param rect Valid pointer to motion state; x, y, dx and dy are modified in place.
 * @note dx/dy are pixels per completed loop, not pixels per second. Motion stays
 *       below the score area. This updates CPU state only, without drawing or SPI.
 */
static void move_rectangle(Rectangle *rect) {
    rect->x += rect->dx;
    rect->y += rect->dy;
    int max_x = DISPLAY_WIDTH - FRAMEBUFFER_RECT_WIDTH;
    int max_y = DISPLAY_HEIGHT - FRAMEBUFFER_RECT_HEIGHT;
    if (rect->x <= 0 || rect->x >= max_x) {
        rect->x = rect->x <= 0 ? 0 : max_x;
        rect->dx = -rect->dx;
    }
    if (rect->y <= FRAMEBUFFER_SCORE_HEIGHT || rect->y >= max_y) {
        rect->y = rect->y <= FRAMEBUFFER_SCORE_HEIGHT ? FRAMEBUFFER_SCORE_HEIGHT : max_y;
        rect->dy = -rect->dy;
    }
}

/**
 * Compose one complete image in the RAM framebuffer.
 * @param rect Valid read-only rectangle state supplying the current position.
 * @param score Two-digit value 0..99: demo counter, or actual MHz in benchmark mode.
 * @note Clears RAM, then draws digits, smile, divider and rectangle. Benchmark
 *       builds also draw a checkerboard. Does not transmit anything to the panel;
 *       the caller sends the finished buffer with st7789_push_frame().
 */
static void draw_frame(const Rectangle *rect, unsigned int score) {
    fb_clear(FB_BLACK);  // Clear RAM only; no intermediate black screen is sent.
    fb_draw_digit(12, 8, score / 10, FB_WHITE);
    fb_draw_digit(28, 8, score % 10, FB_WHITE);
    fb_draw_sprite8(52, 14, smile, FB_GREEN);
#ifdef FPS_BENCHMARK
    // Alternating pixels make colour/transfer corruption easier to notice.
    for (int row = 0; row < 16; row++) {
        for (int column = 0; column < 16; column++) {
            uint16_t color = ((row + column) & 1) ? 0xF800 : 0x001F;
            fb_set_pixel(DISPLAY_WIDTH - 20 + column, 8 + row, color);
        }
    }
#endif
    fb_fill_rect(0, FRAMEBUFFER_SCORE_HEIGHT - 1, DISPLAY_WIDTH, 1, FB_WHITE);
    fb_fill_rect(rect->x, rect->y, FRAMEBUFFER_RECT_WIDTH,
                 FRAMEBUFFER_RECT_HEIGHT, FB_YELLOW);
}

/**
 * Run the single-framebuffer animation and measure completed transfer rate.
 * @return Never returns during normal operation.
 * @note Initializes USB serial and the ST7789, then updates motion, draws into
 *       SRAM and performs one blocking full-screen transfer per loop. Increments
 *       completed_frames only after that transfer returns; no panel acknowledgement
 *       is checked. Reports FPS and average timings after at least one second,
 *       excluding the reporting printf itself from the next timing interval.
 *       Benchmark builds cycle clock stages and show actual SPI MHz as the digits.
 *       Uses one core, no DMA and no deliberate frame delay.
 */
int main(void) {
    stdio_init_all();
    st7789_init();
    Rectangle rect = {0, FRAMEBUFFER_SCORE_HEIGHT, 3, 2};
    unsigned int score = 0;
    unsigned int score_frames = 0;
    unsigned int completed_frames = 0;
    uint64_t draw_total_us = 0;
    uint64_t transfer_total_us = 0;
    uint64_t interval_start = time_us_64();
#ifdef FPS_BENCHMARK
    unsigned int stage = 0;
    benchmark_boot_cpu_khz = clock_get_hz(clk_sys) / 1000;
    select_benchmark_clock(stage);
    uint64_t stage_start = time_us_64();
    interval_start = stage_start;
#endif

    while (true) {
        uint64_t draw_start = time_us_64();
        move_rectangle(&rect);
#ifdef FPS_BENCHMARK
        // The two digits identify the actual clock in MHz: e.g. 09 or 37.
        score = spi_get_baudrate(DISPLAY_SPI) / 1000000;
#endif
        draw_frame(&rect, score);
        uint64_t transfer_start = time_us_64();
        st7789_push_frame(fb_pixels());  // Blocking: CPU waits for the entire frame.
        uint64_t frame_end = time_us_64();
        draw_total_us += transfer_start - draw_start;
        transfer_total_us += frame_end - transfer_start;
        completed_frames++;

        if (++score_frames == 60) {
            score_frames = 0;
            score = (score + 1) % 100;  // Demo counter, not actual game scoring.
        }

        uint64_t elapsed = frame_end - interval_start;
        if (elapsed >= 1000000) {
            double fps = completed_frames * 1000000.0 / elapsed;
            printf("Phase3: %.2f FPS | draw=%u us | SPI=%u us/frame | "
                   "SPI clock=%u Hz | framebuffer=%u bytes\n",
                   fps, (unsigned int)(draw_total_us / completed_frames),
                   (unsigned int)(transfer_total_us / completed_frames),
                   (unsigned int)spi_get_baudrate(DISPLAY_SPI),
                   (unsigned int)(DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t)));
            completed_frames = 0;
            draw_total_us = 0;
            transfer_total_us = 0;
            interval_start = time_us_64();
        }
#ifdef FPS_BENCHMARK
        if (frame_end - stage_start >= FPS_BENCHMARK_STAGE_US) {
            stage = (stage + 1) % (sizeof(benchmark_clocks) / sizeof(benchmark_clocks[0]));
            select_benchmark_clock(stage);  // Change only after the frame is sent.
            completed_frames = 0;
            draw_total_us = 0;
            transfer_total_us = 0;
            stage_start = time_us_64();
            interval_start = stage_start;
        }
#endif
        // No observation delay: this exercise measures unpaced completed frames.
    }
}
