/* Portable timing totals; callers supply actual monotonic timestamps. */
#ifndef CONSOLE_METRICS_H
#define CONSOLE_METRICS_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    float fps;
    unsigned int average_us[4], max_gap_us, max_frame_us, over_budget, dropped_ticks, spi_hz;
} PerfReport;
typedef struct {
    uint64_t epoch, last_sample, totals[4], max_gap, max_frame;
    unsigned int frames, over_budget, dropped_ticks;
} Metrics;
/** Zero timing state and begin the reporting interval at now. */
void metrics_init(Metrics *metrics, uint64_t now);
/** Record one completed frame; stages are input, update, draw, SPI microseconds. */
void metrics_record(Metrics *metrics, uint64_t begin, uint64_t sampled, uint64_t end,
                    const uint64_t stages[4], unsigned int dropped);
/** Produce/reset an interval after at least one second, preserving poll-gap history. */
bool metrics_report(Metrics *metrics, uint64_t now, PerfReport *report);
#endif
