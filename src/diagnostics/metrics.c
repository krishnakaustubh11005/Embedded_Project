/* Frame and sampling metrics, independent of USB and hardware timers. */
#include <string.h>
#include "config.h"
#include "diagnostics/metrics.h"

/** Initialize metrics RAM at the supplied monotonic microsecond timestamp. */
void metrics_init(Metrics *metrics, uint64_t now) {
    memset(metrics,0,sizeof(*metrics)); metrics->epoch=now;
}

/** Add one completed transfer and its stage times; no output or sleeps occur.
 * @note Poll gaps include work between frames, including reporting overhead.
 */
void metrics_record(Metrics *metrics, uint64_t begin, uint64_t sampled, uint64_t end,
                    const uint64_t stages[4], unsigned int dropped) {
    metrics->frames++;
    for (unsigned int i=0;i<4;i++) metrics->totals[i]+=stages[i];
    if (end-begin>metrics->max_frame) metrics->max_frame=end-begin;
    if (end-begin>FRAME_BUDGET_US) metrics->over_budget++;
    if (metrics->last_sample && sampled-metrics->last_sample>metrics->max_gap) {
        metrics->max_gap=sampled-metrics->last_sample;
    }
    metrics->last_sample=sampled;
    metrics->dropped_ticks+=dropped;
}

/** Produce an interval report when ready, leaving report.spi_hz for the board caller.
 * @return True after resetting interval totals, false before a full second.
 * @note The new epoch is now, so later USB report time contributes to next FPS.
 */
bool metrics_report(Metrics *metrics, uint64_t now, PerfReport *report) {
    uint64_t elapsed=now-metrics->epoch;
    if (elapsed<1000000 || !metrics->frames) return false;
    report->fps=(float)(metrics->frames*1000000.0/elapsed);
    for (unsigned int i=0;i<4;i++) report->average_us[i]=(unsigned int)(metrics->totals[i]/metrics->frames);
    report->max_gap_us=(unsigned int)metrics->max_gap;
    report->max_frame_us=(unsigned int)metrics->max_frame;
    report->over_budget=metrics->over_budget;
    report->dropped_ticks=metrics->dropped_ticks;
    uint64_t last=metrics->last_sample;
    metrics_init(metrics,now);
    metrics->last_sample=last;
    return true;
}
