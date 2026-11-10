/* Pure controller processing, separate from ADC switching and GPIO reads. */
#include <math.h>
#include <string.h>
#include "config.h"
#include "platform/input.h"

/** Zero processor/calibration RAM; no hardware or allocation is used. */
void input_init(InputProcessor *processor) { memset(processor, 0, sizeof(*processor)); }

/** Reset calibration sums/counts, preserving raw and debounced button history. */
void input_recalibrate(InputProcessor *processor) {
    memset(processor->sums, 0, sizeof(processor->sums));
    processor->snapshot.calibration_samples = 0;
    processor->snapshot.calibrated = false;
    processor->snapshot.centre_valid = 0;
}

/** Convert a 12-bit reading to [-1,1], removing INPUT_DEAD_ZONE around centre. */
static float normalize_axis(uint16_t raw, uint16_t centre) {
    int delta = (int)raw - centre;
    int distance = delta < 0 ? -delta : delta;
    if (distance <= INPUT_DEAD_ZONE) return 0;
    int span = (delta < 0 ? centre : 4095 - centre) - INPUT_DEAD_ZONE;
    if (span <= 0) return 0;
    return copysignf(fminf((float)(distance - INPUT_DEAD_ZONE) / span, 1.0f), (float)delta);
}

/** Debounce controller p's five pressed bits at monotonic microsecond timestamp now.
 * @note Physical transitions between polls can be missed. No sleeps or I/O occur.
 */
static void process_buttons(InputProcessor *processor, unsigned int p, uint8_t raw, uint64_t now) {
    Controller *out = &processor->snapshot.player[p];
    out->pressed = out->released = 0;
    if (!processor->initialized) {
        processor->candidate[p] = out->held = raw;
        for (unsigned int bit = 0; bit < 5; bit++) processor->changed[p][bit] = now;
    }
    for (unsigned int bit = 0; bit < 5; bit++) {
        uint8_t mask = (uint8_t)(1u << bit);
        if ((raw ^ processor->candidate[p]) & mask) {
            processor->candidate[p] ^= mask;
            processor->changed[p][bit] = now;
        }
        if (((raw ^ out->held) & mask) && now - processor->changed[p][bit] >= INPUT_DEBOUNCE_US) {
            out->held ^= mask;
            if (raw & mask) out->pressed |= mask;
            else out->released |= mask;
        }
    }
    out->raw_buttons = raw;
}

/** Process raw arrays at monotonic now, collecting 32 neutral samples per axis.
 * @note Release sticks during calibration. Rail-like centres are flagged and
 *       replaced by 2048; this detects suspicious readings, not module presence.
 */
void input_process(InputProcessor *processor, const uint16_t axes[2][2],
                   const uint8_t buttons[2], uint64_t now) {
    InputSnapshot *out = &processor->snapshot;
    bool collecting = !out->calibrated;
    for (unsigned int p = 0; p < 2; p++) {
        process_buttons(processor, p, buttons[p], now);
        for (unsigned int axis = 0; axis < 2; axis++) {
            out->player[p].raw[axis] = axes[p][axis];
            if (collecting) processor->sums[p][axis] += axes[p][axis];
        }
    }
    if (collecting && ++out->calibration_samples == INPUT_CALIBRATION_SAMPLES) {
        for (unsigned int p = 0; p < 2; p++) {
            for (unsigned int axis = 0; axis < 2; axis++) {
                unsigned int centre = processor->sums[p][axis] / INPUT_CALIBRATION_SAMPLES;
                bool valid = centre >= 512 && centre <= 3583;
                out->player[p].centre[axis] = valid ? centre : 2048;
                if (valid) out->centre_valid |= (uint8_t)(1u << (p * 2 + axis));
            }
        }
        out->calibrated = true;
    }
    for (unsigned int p = 0; p < 2; p++) {
        out->player[p].x = out->calibrated ? normalize_axis(axes[p][0], out->player[p].centre[0]) : 0;
        out->player[p].y = out->calibrated ? normalize_axis(axes[p][1], out->player[p].centre[1]) : 0;
    }
    out->timestamp_us = now;
    processor->initialized = true;
}
