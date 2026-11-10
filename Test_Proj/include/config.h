/* Hardware and timing settings for the learning exercises. */
#ifndef CONFIG_H
#define CONFIG_H

#include "pico/stdlib.h"

/* Use the onboard LED pin supplied by the Pico 2 board definition. */
#define LED_PIN PICO_DEFAULT_LED_PIN
#define BLINK_HALF_PERIOD_MS 100

/* Shield button A: header D2 wired to Pico GP2 (physical pin 4). */
#define BUTTON_A_PIN 2
#define BUTTON_B_PIN 3
#define BUTTON_C_PIN 4
#define BUTTON_D_PIN 5
/*#define BUTTON_E_PIN 6
#define BUTTON_F_PIN 7 */
/* GP8 remains available for the planned lid switch. */
#define JOYSTICK_BUTTON_PIN 9
#define BUTTON_POLL_INTERVAL_MS 100

/* GPIO numbers prepare the pins; channel numbers select ADC inputs. */
#define JOYSTICK_X_PIN 26
#define JOYSTICK_Y_PIN 27
#define JOYSTICK_X_ADC_CHANNEL 0
#define JOYSTICK_Y_ADC_CHANNEL 1
#define JOYSTICK_POLL_INTERVAL_MS 100

/* Replacement 2.8-inch TFT: connector labels CS, SCK, MOSI, DC, RESET. */
#define DISPLAY_CS_PIN 17
#define DISPLAY_SCK_PIN 18
#define DISPLAY_MOSI_PIN 19
#define DISPLAY_DC_PIN 20
#define DISPLAY_RST_PIN 21
#define DISPLAY_PIN_HOLD_MS 5000

/* Confirmed ST7789, native 240x320 panel, displayed in landscape. */
#define DISPLAY_SPI spi0
/* Deliberately slow transfers so screen painting can be observed. */
#ifndef DISPLAY_SPI_HZ
#define DISPLAY_SPI_HZ 100000
#endif
#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240
/* ST7789 landscape: mirror Y (0x80), exchange axes (0x20), RGB order (0x00).
 * The draw dimensions must be swapped together with this orientation bit.
 */
#define DISPLAY_MADCTL 0xA0
/* Student observed white for RGB565 black with inversion enabled. */
#define DISPLAY_INVERT_COLORS 0

#define DEMO_COLOR_HOLD_MS 1000
#define DEMO_FRAME_DELAY_MS 500
#define DEMO_ANIMATION_FRAMES 120
#define DEMO_RECT_WIDTH 32
#define DEMO_RECT_HEIGHT 24

/* Strip diagnostic: subtract the observed 240-pixel square from width 320. */
#define STRIP_TEST_CURRENT_WIDTH 240
#define STRIP_TEST_HOLD_MS 3000

/* Phase 3 layout: leave space above the animation for two score digits. */
#define FRAMEBUFFER_SCORE_HEIGHT 40
#define FRAMEBUFFER_RECT_WIDTH 32
#define FRAMEBUFFER_RECT_HEIGHT 24

/* Performance experiment: high clock steps need visual module validation.
 * Actual divider-selected rates are printed, not assumed from these requests.
 */
#define FPS_BENCHMARK_CLOCKS_HZ {10000000u, 15000000u, 20000000u, 40000000u, 62500000u, 62500000u}
/* Only the last stage changes the CPU/peripheral source clock. At 125 MHz,
 * SPI can divide by two to reach 62.5 MHz; at 150 MHz that rate is unavailable.
 */
#define FPS_BENCHMARK_ALT_CPU_KHZ 125000u
#define FPS_BENCHMARK_STAGE_US 10000000u

/* Button-controlled Snake timing exercise, using the successful clock setup. */
#define SNAKE_CPU_KHZ 125000u
#define SNAKE_CELL_SIZE 8
#define SNAKE_HEADER_HEIGHT 40
#define SNAKE_DEBOUNCE_US 20000u
#define SNAKE_NORMAL_MOVE_US 125000u
#define SNAKE_DEV_MOVE_US 250000u
#define SNAKE_FRAME_BUDGET_US (1000000u / 43u)
#define SNAKE_MAX_CATCHUP_STEPS 4
#define SNAKE_SERIAL_CHARS_PER_FRAME 32

#endif
