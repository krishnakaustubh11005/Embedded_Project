/* Portable two-controller snapshots; axes are right/up positive. */
#ifndef CONSOLE_INPUT_H
#define CONSOLE_INPUT_H
#include <stdbool.h>
#include <stdint.h>
enum { KEY_A=1u, KEY_B=2u, KEY_C=4u, KEY_D=8u, KEY_MENU=16u };
typedef struct {
    uint16_t raw[2], centre[2];
    float x, y;
    uint8_t raw_buttons, held, pressed, released;
} Controller;
typedef struct {
    Controller player[2];
    uint64_t timestamp_us;
    unsigned int calibration_samples;
    bool calibrated;
    uint8_t centre_valid;
} InputSnapshot;
typedef struct {
    InputSnapshot snapshot;
    uint8_t candidate[2];
    uint64_t changed[2][5];
    uint32_t sums[2][2];
    bool initialized;
} InputProcessor;
/** Zero processor state; the first sample establishes initial held buttons. */
void input_init(InputProcessor *processor);
/** Convert raw [player][axis] samples and pressed masks to calibrated/debounced state. */
void input_process(InputProcessor *processor, const uint16_t axes[2][2],
                   const uint8_t buttons[2], uint64_t now);
/** Restart centre collection while preserving button debounce history. */
void input_recalibrate(InputProcessor *processor);
#endif
