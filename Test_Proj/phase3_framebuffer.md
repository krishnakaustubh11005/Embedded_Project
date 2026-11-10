# Phase 3 learning exercise: framebuffer and FPS

The index check is understood: pixel (5,3) at width 320 is array entry 965.
This exercise stays in Test_Proj; production code is not yet migrated.

## Follow the image through the code

1. `src/framebuffer.c` owns one global RGB565 array. At 320x240 it uses
   153600 bytes (150 KiB) of SRAM. Global static storage keeps it off the
   function-call stack. `fb_pixels()` gives the driver a read-only view.
2. `fb_clear`, `fb_set_pixel` and `fb_fill_rect` modify RAM. Clipping prevents
   coordinates outside the visible screen from writing outside the array.
   `int64_t` intermediates prevent overflow when computing rectangle bounds.
3. Digit rows are small binary patterns stored in a const array in flash.
   A bit mask tests whether each font pixel should be drawn. Digits use 4x4
   blocks for readability. The smile is an original 8x8 one-bit sprite;
   zero bits leave the background unchanged.
4. `src/phase3_framebuffer.c` stores motion in a Rectangle struct. A pointer
   lets the update function change its position. `const Rectangle *` lets
   drawing read that state without modifying it. A two-digit demo counter
   increments every 60 completed frames and wraps at 100.
5. `st7789_push_frame` selects the whole window, packs each uint16_t colour
   into high byte then low byte, and sends 128-byte batches with blocking SPI.
   It does not alter the framebuffer. There is no DMA or second core.

The screen receives only the finished image, avoiding the old separate erase
transfer. This does not guarantee no tearing: the panel can still scan while
SPI updates its internal pixel memory. The MCU has one framebuffer and waits
for the transfer before modifying it again.

## Build and flash

From Emb_Project:

```sh
cmake -S Test_Proj -B Test_Proj/build -DPICO_BOARD=pico2
cmake --build Test_Proj/build --target phase3_framebuffer -j2
```

Close the serial monitor, connect in BOOTSEL mode, then run:

```sh
picotool load Test_Proj/build/phase3_framebuffer.uf2
picotool reboot
python3 Test_Proj/serial_monitor.py
```

This target overrides DISPLAY_SPI_HZ to 10000000 (10 MHz) in CMake. Other
targets retain the 100 kHz observation setting. There is no deliberate frame
pause; the transfer itself sets most of the frame time.

## Observe and measure

- Black background, white two-digit counter and divider, green smile, yellow
  rectangle bouncing below the score area. Look for trails or wrong colours.
- Serial reports completed FPS, average drawing time, average SPI transfer
  time, actual configured SPI clock, and framebuffer byte count about once
  per second. Send 5 consecutive lines once running steadily.
- Times are wall-clock microseconds and include interrupt activity. FPS uses
  complete frames over the reporting interval; printing between intervals
  is excluded. Draw time includes the small motion update.
- At 10 MHz, 320*240*16 bits need at least 122.88 ms on the wire, so the ideal
  transfer-only ceiling is about 8.14 FPS. Actual FPS will be lower because
  of drawing, commands, packing and software gaps. This is a calculation,
  not a measured baseline.

## RAM and linker map

```sh
arm-none-eabi-size Test_Proj/build/phase3_framebuffer.elf
arm-none-eabi-nm -S --size-sort Test_Proj/build/phase3_framebuffer.elf
```

The build's `framebuffer` symbol is 0x25800 bytes = 153600, in SRAM at
0x200018b0. The linker map is `Test_Proj/build/phase3_framebuffer.elf.map`;
search for `.bss` and `framebuffer.c.o`. `.bss` holds zero-initialized globals;
read-only digit/sprite data belongs with flash code/constant data. Exact
addresses can change after edits.

GNU size currently reports text=35248, data=0, bss=156244 bytes. These are
build section totals, not measured runtime stack usage or total reserved RAM.

## Software checks and completion

Pico 2 builds passed for this target and the earlier display target. Temporary
host checks passed with AddressSanitizer and UndefinedBehaviorSanitizer for
framebuffer indexing, clipping, extreme rectangle bounds, font/sprite bits,
SPI window endpoints, byte ordering, transfer length and CS release. These
checks do not verify physical display operation.

The student supplied six steady baseline reports on 2026-10-30: 6.17 FPS,
mean draw=1081.83 us/frame, mean SPI=160903 us/frame, actual SPI=9.375 MHz,
framebuffer=153600 bytes. Raw pixel wire time at that actual clock is
131.072 ms, rather than the 122.88 ms estimate at the requested 10 MHz.
See ../docs/measurements.md for the samples and conditions.

The student confirms the score display, green smile and bouncing yellow
rectangle look correct without trails, and understands the score update
interval. Phase 3 practice checks are complete: visual output and the 6.17 FPS
baseline are recorded. Incorporation into the actual project remains separate
work; this result does not mark the production phase complete.
