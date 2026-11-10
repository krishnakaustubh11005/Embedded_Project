/* Pico Arcade: bounded input/commands, fixed updates, SRAM rendering and SPI. */
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "config.h"
#include "platform/board.h"
#include "platform/audio.h"
#include "audio/sound.h"
#include "platform/input.h"
#include "platform/line_parser.h"
#include "graphics/framebuffer.h"
#include "app/app.h"
#include "diagnostics/metrics.h"

/* Large persistent state belongs in SRAM, not the small function-call stack. */
static App app;
static InputProcessor input;
static LineParser commands;

/** Process at most 32 available USB characters; never wait for a full command. */
static void poll_commands(void) {
    for (unsigned int i=0;i<SERIAL_CHARS_PER_FRAME;i++) {
        int character=getchar_timeout_us(0);
        if (character==PICO_ERROR_TIMEOUT) break;
        LineStatus status=line_parser_feed(&commands,(char)character);
        if (status==LINE_READY) printf("[APP] %s\n",app_command(&app,commands.text));
        if (status==LINE_TOO_LONG) printf("[INPUT] Command discarded: line too long\n");
    }
}

/** Print one real timing report; included in the following interval's FPS and gap. */
static void print_report(const PerfReport *report) {
    const char *name=app.selected<GAME_COUNT?game_get(app.selected)->name:"UI";
    printf("[PERF] %s screen=%u %.2f FPS | input=%u update=%u draw=%u SPI=%u us | "
           "frame_max=%u gap_max=%u over_budget=%u dropped=%u | SPI clock=%u Hz | audio blocks=%u%s\n",
           name,(unsigned int)app.screen,(double)report->fps,
           report->average_us[0],report->average_us[1],report->average_us[2],report->average_us[3],
           report->max_frame_us,report->max_gap_us,report->over_budget,report->dropped_ticks,report->spi_hz,
           audio_blocks(),sound_muted()?" (muted)":"");
}

/**
 * Run the production four-game console with the approved wiring and clock setup.
 * @return Never returns during normal operation.
 * @note Reads both controllers, routes new events once, runs bounded 120 Hz
 *       updates, composes one SRAM image and blocks until SPI completes. Timing
 *       and commands use USB; play requires no serial monitor. Audio runs
 *       separately from DMA interrupts (see audio_pwm.c); there is no heap,
 *       flash persistence or second-core shared state in this release.
 */
int main(void) {
    board_init();
    sound_init();
    audio_init();
    input_init(&input);
    uint64_t previous=time_us_64();
    app_init(&app,(uint32_t)previous);
    Metrics metrics;
    metrics_init(&metrics,previous);
    PerfReport report={.spi_hz=board_spi_hz()};
    printf("[BOOT] Pico Arcade CPU=%u peri=%u SPI=%u Hz; type HELP\n",
        (unsigned int)clock_get_hz(clk_sys),(unsigned int)clock_get_hz(clk_peri),report.spi_hz);
    while (true) {
        uint64_t begin=time_us_64();
        uint16_t axes[2][2]; uint8_t buttons[2];
        board_read_inputs(axes,buttons);
        input_process(&input,axes,buttons,time_us_64());
        uint64_t sampled=time_us_64();
        poll_commands();
        app_input(&app,&input.snapshot);
        if (app.recalibrate_requested) {
            input_recalibrate(&input); app.recalibrate_requested=false;
        }
        unsigned int dropped=app_update(&app,&input.snapshot,sampled-previous);
        previous=sampled;
        uint64_t updated=time_us_64();
        app_draw(&app,&input.snapshot,&report);
        uint64_t drawn=time_us_64();
        board_present(fb_pixels());
        uint64_t end=time_us_64();
        uint64_t stages[]={sampled-begin,updated-sampled,drawn-updated,end-drawn};
        metrics_record(&metrics,begin,sampled,end,stages,dropped);
        if (metrics_report(&metrics,end,&report)) print_report(&report);
    }
}
