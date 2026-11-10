/* Minimal ILI9341 commands and RGB565 fills; no third-party graphics library.
 * Command definitions: ILI9341 datasheet section 8.2 and its reset timing tables:
 * https://cdn-shop.adafruit.com/datasheets/ILI9341.pdf
 * Power/gamma initialization reference:
 * https://github.com/adafruit/Adafruit_ILI9341/blob/master/Adafruit_ILI9341.cpp
 */
#include <stddef.h>
#include <stdint.h>

#include "hardware/spi.h"
#include "pico/stdlib.h"
#include "config.h"
#include "ili9341.h"

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
 * Initialize the ILI9341, its SPI peripheral and control GPIOs.
 * @note Uses config.h pin assignments, clock, orientation and inversion setting.
 *       Pulses hardware reset and sends the controller-specific startup commands
 *       with blocking recovery delays. Call before any drawing/transfer function.
 *       Changes hardware state and does not verify a panel acknowledgement.
 */
void ili9341_init(void) {
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

    /* The module may need explicit internal power/timing/gamma setup before
     * its pixel memory can drive the panel. These values follow the published
     * Adafruit ILI9341 initialization table; this remains our own small driver.
     */
    const uint8_t ext1[] = {0x03, 0x80, 0x02};
    const uint8_t power_seq[] = {0x00, 0xC1, 0x30};
    const uint8_t power_on[] = {0x64, 0x03, 0x12, 0x81};
    const uint8_t driver_timing[] = {0x85, 0x00, 0x78};
    const uint8_t pump_ratio[] = {0x39, 0x2C, 0x00, 0x34, 0x02};
    const uint8_t pump_control = 0x20;
    const uint8_t driver_timing2[] = {0x00, 0x00};
    const uint8_t power1 = 0x23;
    const uint8_t power2 = 0x10;
    const uint8_t vcom1[] = {0x3E, 0x28};
    const uint8_t vcom2 = 0x86;
    const uint8_t scroll_start[] = {0x00, 0x00};
    const uint8_t frame_rate[] = {0x00, 0x18};
    const uint8_t display_function[] = {0x08, 0x82, 0x27};
    const uint8_t gamma_disable = 0x00;
    const uint8_t gamma_curve = 0x01;
    const uint8_t gamma_positive[] = {
        0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1,
        0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00
    };
    const uint8_t gamma_negative[] = {
        0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1,
        0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F
    };
    const uint8_t pixel_format = 0x55;  // COLMOD: 16-bit RGB565.
    const uint8_t orientation = DISPLAY_MADCTL;

    write_command(0xEF, ext1, sizeof(ext1));
    write_command(0xCF, power_seq, sizeof(power_seq));
    write_command(0xED, power_on, sizeof(power_on));
    write_command(0xE8, driver_timing, sizeof(driver_timing));
    write_command(0xCB, pump_ratio, sizeof(pump_ratio));
    write_command(0xF7, &pump_control, 1);
    write_command(0xEA, driver_timing2, sizeof(driver_timing2));
    write_command(0xC0, &power1, 1);
    write_command(0xC1, &power2, 1);
    write_command(0xC5, vcom1, sizeof(vcom1));
    write_command(0xC7, &vcom2, 1);
    write_command(0x36, &orientation, 1);  // MADCTL: scan direction/colour order.
    write_command(0x37, scroll_start, sizeof(scroll_start));
    write_command(0x3A, &pixel_format, 1);
    write_command(0xB1, frame_rate, sizeof(frame_rate));
    write_command(0xB6, display_function, sizeof(display_function));
    write_command(0xF2, &gamma_disable, 1);
    write_command(0x26, &gamma_curve, 1);
    write_command(0xE0, gamma_positive, sizeof(gamma_positive));
    write_command(0xE1, gamma_negative, sizeof(gamma_negative));

    write_command(0x11, NULL, 0);  // Sleep out: start internal panel operation.
    sleep_ms(150);

    if (DISPLAY_INVERT_COLORS) {
        write_command(0x21, NULL, 0);  // Display inversion on.
    } else {
        write_command(0x20, NULL, 0);  // Display inversion off.
    }
    write_command(0x29, NULL, 0);  // Display on: show the controller's pixel memory.
    sleep_ms(150);  // Allow panel output to stabilize after Display On.
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
void ili9341_fill_rect(unsigned int x, unsigned int y,
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
void ili9341_fill_screen(uint16_t color) {
    ili9341_fill_rect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, color);
}
