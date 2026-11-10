/* Pico-only clock, GPIO, ADC and display boundary. */
#ifndef CONSOLE_BOARD_H
#define CONSOLE_BOARD_H
#include <stdint.h>
/** Configure clocks, USB, switch controls, ADC, buttons and display; reset blocks. */
void board_init(void);
/** Sample individually switched X and direct Y axes; both switches finish off. */
void board_read_inputs(uint16_t axes[2][2], uint8_t buttons[2]);
/** Send the stable full-size framebuffer through blocking SPI. */
void board_present(const uint16_t *pixels);
/** Read the divider-selected SPI rate, not an external electrical measurement. */
unsigned int board_spi_hz(void);
#endif
