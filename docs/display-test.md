# ST7789 standalone visual test

The current display is the replacement 2.8-inch 240x320 module, marked V1.1
ROHS. On 2026-10-30 the student confirmed its LCD controller is ST7789.
The old SmartElex 3.2-inch module and ILI9341 driver are historical hardware.

## Wiring and configuration

| Display label | Pico GPIO | Physical pin |
|---|---|---|
| CS | GP17 | 22 |
| SCK | GP18 | 24 |
| MOSI | GP19 | 25 |
| DC | GP20 | 26 |
| RESET | GP21 | 27 |
| GND | GND | 38 |

Keep the replacement module's working supply connection. Its precise VCC
requirement has not been documented here. The old SmartElex IM strap and
5 V backlight instructions do not apply to this replacement. Pico outputs
use 3.3 V logic. Touch and SD interfaces are outside this exercise.

`Test_Proj/include/config.h` selects 320x240 landscape, SPI mode 0 at 100 kHz,
MADCTL=0xA0 (row/column exchange and Y mirror, RGB order), and inversion off.
The full native 240x320 panel uses no address offset. Orientation and colour
settings still require visual confirmation on this module.

## Driver and embedded concepts

`Test_Proj/src/st7789.c` supplies blocking SPI initialization, window selection
and RGB565 fills. Both display targets now link this file, not `ili9341.c`.
There is no external graphics library dependency.

Initialization configures how the controller uses its pixel memory. The
ST7789 sequence sends software reset (0x01), sleep out (0x11), RGB565 pixel
format (0x3A/0x55), memory access control (0x36), inversion selection
(0x21/0x20), normal display mode (0x13) and display on (0x29), with recovery
waits. ILI9341-specific power/gamma commands are absent from this driver.

Each rectangle sets inclusive column and row bounds (0x2A/0x2B), then sends
pixel bytes (0x2C), high byte first. A 128-byte transfer batch avoids a full
framebuffer. The panel stores the pixels; the CPU waits for SPI transfers.

References: [ST7789 initialization and rotation example](https://github.com/adafruit/Adafruit-ST7735-Library/blob/master/Adafruit_ST7789.cpp),
[Sitronix ST7789V2 datasheet](https://files.waveshare.com/wiki/common/ST7789V2.pdf).

## Build and flash

From `Emb_Project/`:

```sh
cmake -S Test_Proj -B Test_Proj/build -DPICO_BOARD=pico2
cmake --build Test_Proj/build --target phase2_display phase2_strip_test -j2
```

Close any monitor, connect in BOOTSEL mode, then flash the full test:

```sh
picotool load Test_Proj/build/phase2_display.uf2
picotool reboot
python3 Test_Proj/serial_monitor.py
```

For the separate strip test, flash `Test_Proj/build/phase2_strip_test.uf2`.

## What to observe

The full test repeats red, green, blue, a white border at all four screen
edges, and a bouncing yellow rectangle. The strip test draws a blue reference
at X=0..239, then targets X=240..319 with yellow and black, with 3-second holds.
A blank strip still does not establish a hardware fault without confirming
controller setup. Serial output describes sent commands; it is not readback.

The current learning configuration deliberately slows transfers: a full RGB565
screen sends 320*240*16 bits, taking about 12.3 seconds at 100 kHz before other
overhead. Rectangle motion steps by one pixel in each axis, with a 500 ms pause
per update in addition to transfer time. The old rectangle is erased before
the new one is drawn, so visible flicker is expected during this observation.

Report full coverage, border visibility, colours, and rectangle motion. If
red and blue are swapped, toggle only the BGR bit (0x08) in DISPLAY_MADCTL
and rebuild. If colours appear complemented, test the inversion option.
No successful full-screen result is assumed until the student reports it.
