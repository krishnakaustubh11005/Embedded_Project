# Phase 3 performance experiment

Hardware update (2026-10-31): student supplied nine samples each for stages
4 and 5: all report 27.05 FPS and actual SPI=37.5 MHz. Draw is 1122..1123 us
and transfer 35846 us/frame. Image stability remains unconfirmed.

The measured baseline remains 6.17 FPS with 8-bit byte batches at actual
9.375 MHz. `phase3_framebuffer` retains that transfer implementation.

`phase3_fps_benchmark` uses the same framebuffer, drawing code and blocking
single-core approach, but transmits pixels as 16-bit MSB-first SPI words.
This sends RGB565 directly from RAM, removing the byte-packing loop and
1200 separate 128-byte pixel-write calls per full frame. Commands are still
8-bit: the driver switches to 16-bit after Memory Write and restores 8-bit
after the pixel transfer has completed and CS is released.

The updated target has six stages, holding each for 10 seconds. The first
five request 10, 15, 20, 40 and 62.5 MHz using the boot CPU clock. At the
current 150 MHz peripheral source, actual rates are 9.375, 15, 18.75, 37.5
and 37.5 MHz. The earlier predicted 50 MHz final rate was incorrect: the
SPI prescale is even, with an additional integer divider, so 150/3=50 MHz
is unavailable. The next higher available rate is 75 MHz, above the request.

Stage 6 changes CPU and peripheral clocks to 125 MHz and requests SPI=62.5 MHz,
which is attainable with a divide-by-two. This lowers CPU frequency rather
than overclocking it. The benchmark stops SPI before source changes, uses
the SDK system-clock helper, explicitly reconnects clk_peri to clk_sys, and
reinitializes SPI to 8-bit mode 0. USB uses its independent clock. Each header
prints CPU, peripheral and actual SPI clocks. The next sweep restores the
boot CPU clock. This changes CPU speed as well as SPI; compare draw time too.
Stage 6 update: the student reports a correct display and maximum around
42 FPS using the alternative source-clock setup. Exact stage 6 serial lines
remain outstanding. This is a successful visual trial, not an absolute module
frequency-limit measurement.

The first stage separates transfer-code improvement from clock improvement:
compare its FPS with the 6.17 FPS baseline at the same reported actual clock.

## Clock limits and visual validation

Exact ST7789 silicon suffix and board timing are not confirmed. The
[ST7789V datasheet, serial timing table](https://dl.espressif.com/dl/schematics/ST7789V_SPEC_V1.0.pdf)
specifies a 66 ns minimum write cycle (about 15.15 MHz), whereas the
[ST7789VW datasheet](https://download.mikroe.com/documents/datasheets/ST7789V_datasheet.pdf)
lists 16 ns (62.5 MHz). Steps above actual 15 MHz are module-specific
experiments, not guaranteed operating points for an unidentified suffix.
Select the highest tested rate that has a correct, stable image; this short
sweep alone does not establish long-term reliability.

## Run

From Emb_Project, build if needed:

```sh
cmake -S Test_Proj -B Test_Proj/build -DPICO_BOARD=pico2
cmake --build Test_Proj/build --target phase3_fps_benchmark -j2
```

Connect in BOOTSEL mode and flash:

```sh
picotool load Test_Proj/build/phase3_fps_benchmark.uf2
picotool reboot
python3 Test_Proj/serial_monitor.py
```

Observe at least one complete sweep (about 60 seconds). Digits now identify
the actual clock truncated to MHz (09, 15, 18, 37, 37, 62), rather than the demo
score. A 16x16 red/blue checkerboard at top right provides a pattern to inspect
for missing pixels or colour errors. The smile, divider and moving yellow
rectangle should also remain correct at every stage.

Send BENCH headers and FPS lines for all six stages, and identify any stage
with corruption, blanking, missing rows or a frozen image. Serial throughput
alone cannot verify whether the display received correct data. The fastest
stable rate remains pending until hardware feedback; no FPS improvement has
yet been measured. DMA and partial redraw are separate later experiments.

Both firmware targets compile. Temporary host SPI mocks passed for original
and optimized paths, including window addresses, MSB-first word values,
complete pixel count, CS release and restoration of the command word size.
These tests model SDK calls and do not verify electrical timing.
