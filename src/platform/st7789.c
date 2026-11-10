/* Small ST7789 blocking-SPI driver: init, window selection, RGB565 fills.
 * Init/rotation reference (no external graphics library dependency):
 * https://github.com/adafruit/Adafruit-ST7735-Library/blob/master/Adafruit_ST7789.cpp
 * Command reference: Sitronix ST7789V2 datasheet, section 9:
 * https://files.waveshare.com/wiki/common/ST7789V2.pdf
 * Full 240x320 panel: zero address offset, rotated to 320x240 in config.h.
 */
#include <stddef.h>
#include <stdint.h>

#include "hardware/spi.h"
#include "pico/stdlib.h"
#include "config.h"
#include "platform/st7789.h"

/* data points to parameter bytes; NULL with count=0 means no parameters. */
/**
 * Send one command and its optional parameter bytes over blocking SPI.
 * @param command Controller command byte; DC is held low for this byte.
 * @param data Parameter array; may be NULL only when count is zero.
 * @param count Number of parameter bytes, sent with DC high.
 * @note Selects the display with CS low and releases CS before returning.
 *       Requires initialized SPI in 8-bit mode; does not read an acknowledgement.
 */
static void write_command(uint8_t command, const uint8_t *data, size_t count) {
    gpio_put(DISPLAY_CS_PIN, 0);  // Select the display for this transaction.
    gpio_put(DISPLAY_DC_PIN, 0);  // The first byte is a command.
    spi_write_blocking(DISPLAY_SPI, &command, 1);

    if (count > 0) {
        gpio_put(DISPLAY_DC_PIN, 1);  // Remaining bytes are command parameters.
        spi_write_blocking(DISPLAY_SPI, data, count);
    }

    gpio_put(DISPLAY_CS_PIN, 1);  // Blocking writes finish before deselection.
}

/**
 * Prepare a GPIO output with a known initial level.
 * @param pin Pico GPIO number from config.h, not a physical header-pin number.
 * @param level Initial logic level: false/0 is low, true/1 is high.
 * @note Loads the output latch before enabling its driver to avoid an unwanted pulse.
 */
static void setup_output(unsigned int pin, bool level) {
    gpio_init(pin);
    gpio_put(pin, level);  // Load the intended level before enabling output.
    gpio_set_dir(pin, GPIO_OUT);
}

/**
 * Initialize the ST7789, its SPI peripheral and control GPIOs.
 * @note Uses config.h pin assignments, clock, orientation and inversion setting.
 *       Pulses hardware reset and sends the controller-specific startup commands
 *       with blocking recovery delays. Call before any drawing/transfer function.
 *       Changes hardware state and does not verify a panel acknowledgement.
 */
void st7789_init(void) {
    setup_output(DISPLAY_CS_PIN, 1);
    setup_output(DISPLAY_DC_PIN, 0);
    setup_output(DISPLAY_RST_PIN, 1);

    spi_init(DISPLAY_SPI, DISPLAY_SPI_HZ);
    // 8-bit bytes, clock idle low, sample on the first (rising) edge, MSB first.
    spi_set_format(DISPLAY_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(DISPLAY_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(DISPLAY_MOSI_PIN, GPIO_FUNC_SPI);

    sleep_ms(20);
    gpio_put(DISPLAY_RST_PIN, 0);  // Hardware reset is active low.
    sleep_ms(20);
    gpio_put(DISPLAY_RST_PIN, 1);
    sleep_ms(150);  // Allow reset recovery before sending commands.

    write_command(0x01, NULL, 0);  // SWRESET: discard any previous setup.
    sleep_ms(150);
    write_command(0x11, NULL, 0);  // SLPOUT: enable internal panel operation.
    sleep_ms(150);  // Conservative recovery delay before further commands.

    const uint8_t pixel_format = 0x55;  // COLMOD: 16-bit RGB565.
    write_command(0x3A, &pixel_format, 1);
    sleep_ms(10);
    const uint8_t orientation = DISPLAY_MADCTL;
    write_command(0x36, &orientation, 1);  // MADCTL: scan axes and colour order.

    // Many ST7789 panels use inversion; this is separate from RGB/BGR order.
    write_command(DISPLAY_INVERT_COLORS ? 0x21 : 0x20, NULL, 0);
    sleep_ms(10);
    write_command(0x13, NULL, 0);  // NORON: use the full normal display area.
    sleep_ms(10);
    write_command(0x29, NULL, 0);  // DISPON: enable visible panel output.
    sleep_ms(150);
}

/**
 * Set the inclusive column and row bounds for a subsequent pixel write.
 * @param x First column in the configured display orientation.
 * @param y First row in the configured display orientation.
 * @param width Number of columns; must be nonzero and fit the screen.
 * @param height Number of rows; must be nonzero and fit the screen.
 * @note Sends address commands over blocking SPI. Callers must clip coordinates
 *       first. This function selects the window but does not send pixel colours.
 */
static void set_window(unsigned int x, unsigned int y,
                       unsigned int width, unsigned int height) {
    unsigned int last_x = x + width - 1;  // End coordinates are inclusive.
    unsigned int last_y = y + height - 1;

    // >> 8 selects the upper byte; the uint8_t cast keeps the lower 8 bits.
    const uint8_t columns[] = {
        (uint8_t)(x >> 8), (uint8_t)x,
        (uint8_t)(last_x >> 8), (uint8_t)last_x
    };
    const uint8_t rows[] = {
        (uint8_t)(y >> 8), (uint8_t)y,
        (uint8_t)(last_y >> 8), (uint8_t)last_y
    };
    write_command(0x2A, columns, sizeof(columns));  // Column address set.
    write_command(0x2B, rows, sizeof(rows));        // Page/row address set.
}

/**
 * Send one solid RGB565 rectangle directly to the display.
 * @param x First column in the configured orientation.
 * @param y First row in the configured orientation.
 * @param width Requested width in pixels.
 * @param height Requested height in pixels.
 * @param color Packed 16-bit RGB565 colour, sent high byte first.
 * @note Empty/off-screen rectangles are ignored; right and bottom edges are
 *       clipped. Uses a small repeated-colour batch, blocking SPI and no full
 *       framebuffer. Requires the controller to have been initialized.
 */
void st7789_fill_rect(unsigned int x, unsigned int y,
                       unsigned int width, unsigned int height, uint16_t color) {
    if (width == 0 || height == 0 || x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) {
        return;  // Empty or entirely off-screen rectangles produce no transfer.
    }
    // Clip before computing end coordinates or the number of pixels.
    if (width > DISPLAY_WIDTH - x) {
        width = DISPLAY_WIDTH - x;
    }
    if (height > DISPLAY_HEIGHT - y) {
        height = DISPLAY_HEIGHT - y;
    }
    set_window(x, y, width, height);

    uint8_t pixels[128];  // A small batch of 64 identical, two-byte pixels.
    for (size_t i = 0; i < sizeof(pixels); i += 2) {
        pixels[i] = (uint8_t)(color >> 8);  // RGB565 upper byte goes first.
        pixels[i + 1] = (uint8_t)color;
    }

    const uint8_t memory_write = 0x2C;
    gpio_put(DISPLAY_CS_PIN, 0);
    gpio_put(DISPLAY_DC_PIN, 0);
    spi_write_blocking(DISPLAY_SPI, &memory_write, 1);
    gpio_put(DISPLAY_DC_PIN, 1);  // All following bytes are pixel data.

    size_t remaining = width * height;
    const size_t batch_pixels = sizeof(pixels) / 2;
    while (remaining > 0) {
        size_t count = remaining < batch_pixels ? remaining : batch_pixels;
        spi_write_blocking(DISPLAY_SPI, pixels, count * 2);
        remaining -= count;
    }
    gpio_put(DISPLAY_CS_PIN, 1);
}

/**
 * Fill the entire configured display with one RGB565 colour.
 * @param color Packed colour repeated for every visible pixel.
 * @note Calls the direct rectangle writer and blocks until SPI finishes.
 *       Does not modify the separate framebuffer used in Phase 3.
 */
void st7789_fill_screen(uint16_t color) {
    st7789_fill_rect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, color);
}

/**
 * Send one complete RAM image to the full ST7789 address window.
 * @param pixels Valid pointer to DISPLAY_WIDTH * DISPLAY_HEIGHT RGB565 values,
 *               in row-major order; the caller owns the array and keeps it stable.
 * @note Blocks until the final pixel bits leave SPI, then releases CS. Default
 *       builds pack high/low bytes into small batches; FRAME_TRANSFER_16BIT builds
 *       feed uint16_t pixels directly using MSB-first 16-bit SPI, then restore
 *       8-bit command mode. The source is not modified or copied as a full frame.
 *       Completion confirms Pico transmission, not correct physical display output.
 */
void st7789_push_frame(const uint16_t *pixels) {
    set_window(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    const uint8_t memory_write = 0x2C;
    gpio_put(DISPLAY_CS_PIN, 0);
    gpio_put(DISPLAY_DC_PIN, 0);
    spi_write_blocking(DISPLAY_SPI, &memory_write, 1);
    gpio_put(DISPLAY_DC_PIN, 1);

#ifdef FRAME_TRANSFER_16BIT
    /* RGB565 already occupies 16 bits. MSB-first 16-bit SPI sends the high
     * byte first directly from the native uint16_t array; no copy is needed.
     * The SDK call waits until the last word leaves the peripheral.
     */
    spi_set_format(DISPLAY_SPI, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    spi_write16_blocking(DISPLAY_SPI, pixels, DISPLAY_WIDTH * DISPLAY_HEIGHT);
    gpio_put(DISPLAY_CS_PIN, 1);
    // Subsequent commands and their parameters still require 8-bit words.
    spi_set_format(DISPLAY_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
#else
    /* The CPU stores uint16_t values in its native byte order. Pack them into
     * display byte order without changing the framebuffer itself.
     */
    uint8_t batch[128];
    size_t offset = 0;
    const size_t pixel_count = DISPLAY_WIDTH * DISPLAY_HEIGHT;
    while (offset < pixel_count) {
        size_t count = pixel_count - offset;
        if (count > sizeof(batch) / 2) {
            count = sizeof(batch) / 2;
        }
        for (size_t i = 0; i < count; i++) {
            uint16_t color = pixels[offset + i];
            batch[2 * i] = (uint8_t)(color >> 8);
            batch[2 * i + 1] = (uint8_t)color;
        }
        spi_write_blocking(DISPLAY_SPI, batch, count * 2);
        offset += count;
    }
    gpio_put(DISPLAY_CS_PIN, 1);
#endif
}
