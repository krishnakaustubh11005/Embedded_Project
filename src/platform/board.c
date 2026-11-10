/* Pico hardware boundary with isolated, sequential shared-X ADC sampling. */
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/regs/clocks.h"
#include "hardware/spi.h"
#include "config.h"
#include "platform/board.h"
#include "platform/st7789.h"
static const unsigned int pins[2][5] = {P1_BUTTON_PINS, P2_BUTTON_PINS};

/** Break both shared-X paths before enabling P1 (0), P2 (1), or neither (-1).
 * @note IC controls 13/5 connect to GP14/15; both are never high together.
 */
static void select_x_axis(int player) {
    gpio_put(P1_X_ENABLE_PIN, 0);
    gpio_put(P2_X_ENABLE_PIN, 0);
    if (player < 0) return;
    busy_wait_us_32(MUX_BREAK_US);
    gpio_put(player == 0 ? P1_X_ENABLE_PIN : P2_X_ENABLE_PIN, 1);
}

/** Select ADC channel 0/1/2, wait settle_us microseconds, discard stale
 * conversions, then return an averaged raw 12-bit value (0..4095).
 * @note Settling happens after ADC selection, not just after mux selection.
 */
static uint16_t sample_adc(unsigned int channel, uint32_t settle_us) {
    adc_select_input(channel);
    busy_wait_us_32(settle_us);
    for (unsigned int i = 0; i < ADC_DISCARD_SAMPLES; i++) (void)adc_read();
    uint32_t sum = 0;
    for (unsigned int i = 0; i < ADC_AVERAGE_SAMPLES; i++) sum += adc_read();
    return (uint16_t)(sum / ADC_AVERAGE_SAMPLES);
}

/** Read player 0/1 X as raw 0..4095 after break-before-make and settling.
 * @note Only the selected joystick drives ADC2; both switches finish off.
 */
static uint16_t sample_x_axis(unsigned int player) {
    select_x_axis((int)player);
    uint16_t value = sample_adc(SHARED_X_ADC, MUX_SETTLE_US);
    select_x_axis(-1);
    return value;
}

/** Initialize 125 MHz CPU/peripheral, USB, ADC, buttons and the ST7789.
 * @note Preloads switch outputs low. Display startup includes blocking waits.
 */
void board_init(void) {
    set_sys_clock_khz(CONSOLE_CPU_KHZ, true);
    clock_configure_undivided(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
                             clock_get_hz(clk_sys));
    stdio_init_all();
    const unsigned int controls[] = {P1_X_ENABLE_PIN, P2_X_ENABLE_PIN};
    for (unsigned int i = 0; i < 2; i++) {
        gpio_init(controls[i]);
        gpio_put(controls[i], 0);
        gpio_set_dir(controls[i], GPIO_OUT);
    }
    for (unsigned int p = 0; p < 2; p++) {
        for (unsigned int bit = 0; bit < 5; bit++) {
            gpio_init(pins[p][bit]);
            gpio_set_dir(pins[p][bit], GPIO_IN);
            gpio_pull_up(pins[p][bit]);
        }
    }
    adc_init();
    adc_gpio_init(SHARED_X_ADC_PIN);
    adc_gpio_init(P1_Y_PIN);
    adc_gpio_init(P2_Y_PIN);
    st7789_init();
}

/** Sample buttons and four axes sequentially into caller-owned arrays.
 * @note Includes bounded ADC/settling waits. Samples are not simultaneous;
 *       button mask one means pressed. Leaves both shared-X controls low.
 */
void board_read_inputs(uint16_t axes[2][2], uint8_t buttons[2]) {
    for (unsigned int p = 0; p < 2; p++) {
        buttons[p] = 0;
        for (unsigned int bit = 0; bit < 5; bit++) {
            if (!gpio_get(pins[p][bit])) buttons[p] |= (uint8_t)(1u << bit);
        }
    }
    axes[0][0] = sample_x_axis(0);
    axes[0][1] = sample_adc(P1_Y_ADC, ADC_SETTLE_US);
    axes[1][1] = sample_adc(P2_Y_ADC, ADC_SETTLE_US);
    axes[1][0] = sample_x_axis(1);
}

/** Transmit pixels without modifying RAM; waits until the final bit is sent. */
void board_present(const uint16_t *pixels) { st7789_push_frame(pixels); }

/** Return the SPI divider-selected baud rate; does not verify display reception. */
unsigned int board_spi_hz(void) { return spi_get_baudrate(DISPLAY_SPI); }
