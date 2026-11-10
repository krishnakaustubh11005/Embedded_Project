# Performance benchmark observation — 2026-10-31

## Snake timing exercise preparation — 2026-10-31

- Control preference update: student requests A=up, B=right, C=down, D=left.
  Pin wiring stays GP2/3/4/5. Updated explicit button-direction lookup, serial
  banner, screen hints and walkthrough; rebuilt UF2. This supersedes the
  original direction assignment below. Hardware direction check pending.

- Student confirms A/B/C/D are simultaneously wired to GP2/3/4/5; assigned
  up/down/left/right respectively. Snake uses buttons rather than joystick ADC.
- Prepared separate Test_Proj phase4_snake with normal eight-cell/s movement,
  debounced inputs, ball food, growth/score, collisions, restart and USB commands.
  DEV 2350 enables four-cell/s movement and wall wrapping; DEV OFF restores
  normal rules. Uses 125 MHz CPU/peripheral and requested 62.5 MHz SPI.
- Added stage timing, controls/frame/input-gap maxima and 43-FPS-budget misses.
  Game FPS and input response have not been measured on hardware yet.
- Pico build and host rule/input tests passed; host renderer pixel checks and
  a visual layout inspection passed. No hardware game result is assumed.

## Benchmark reports

- Student confirms the display remains visually correct in the alternative
  125 MHz CPU/peripheral clock test and reports a maximum around 42 FPS.
  This stage requests SPI=62.5 MHz (125 MHz / 2). Exact stage 6 serial timing
  lines and sustained FPS distribution have not been supplied. Treat 62.5 MHz
  as the highest visually successful test setting so far, not proof of the
  module's absolute frequency limit or long-term stability.

- Follow-up: nine samples each for stages 4 and 5 report 27.05 FPS, SPI transfer
  35846 us/frame and framebuffer=153600 bytes. Drawing is 1122..1123 us/frame.
  Stage 4 request=40 MHz and stage 5 request=62.5 MHz both report actual=37.5 MHz.
  Their equal performance therefore does not establish a display FPS cap.
- At 37.5 MHz, calculated raw-pixel wire time=32768 us. About 3078 us remains
  inside the transfer routine beyond raw pixel wire time; not individually
  profiled. Combined draw/transfer is about 36969 us, consistent with 27.05 FPS.
- Correction: earlier expectation that a 150 MHz source could yield 50 MHz
  SPI was wrong. The even prescaler times integer divider cannot divide by 3.
- Added a sixth benchmark stage: CPU/peripheral=125 MHz, requested SPI=62.5 MHz.
  Both targets build and mocked clock-switch ordering checks passed. Physical
  clock-switch stability, stage 6 FPS and image correctness were initially
  pending; see the subsequent user observation above.

- Student reports a maximum observed 27 FPS in the optimized clock-sweep test.
  This is approximately 4.38 times the earlier 6.17 FPS baseline, or about
  37 ms per completed frame. It combines transfer-code and clock changes;
  their individual contributions have not yet been established.
- Initially matching actual SPI clock, draw/transfer timings, sustained rate and
  visual stability had not been supplied. Visual stability is still pending.
  Do not select a final clock
  or call this a verified stable maximum until those details are available.

# Confirmed replacement controller — 2026-10-30

## Phase 3 measured baseline — 2026-10-30

Performance follow-up: student requested maximum FPS before Phase 4. Prepared
separate phase3_fps_benchmark, using direct 16-bit blocking SPI and a five-stage
clock sweep. Baseline target retains 8-bit packing. Both builds and host SPI
contract checks passed. New FPS, highest visually stable clock and precise
controller suffix are pending; no speedup is claimed from software checks.

Conditions: Pico 2, 2.8-inch ST7789, 320x240 RGB565, Test_Proj
phase3_framebuffer, one SRAM buffer, one core, blocking full-frame SPI,
no deliberate frame delay. Requested clock 10 MHz; reported actual clock
9.375 MHz. Six consecutive serial samples supplied by student:

| FPS | Draw (us/frame) | SPI (us/frame) | Actual SPI (Hz) | Framebuffer (bytes) |
|---|---|---|---|---|
| 6.17 | 1079 | 160908 | 9375000 | 153600 |
| 6.17 | 1086 | 160902 | 9375000 | 153600 |
| 6.17 | 1083 | 160902 | 9375000 | 153600 |
| 6.17 | 1081 | 160902 | 9375000 | 153600 |
| 6.17 | 1081 | 160902 | 9375000 | 153600 |
| 6.17 | 1081 | 160902 | 9375000 | 153600 |

Mean of reported interval averages: draw=1081.83 us, SPI=160903 us.
Combined average about 161.985 ms/frame; SPI is about 99.33% of that time.
Raw pixel wire time at the actual clock is calculated as 131.072 ms; measured
SPI routine time includes byte packing, commands, batch gaps and interrupts.
The approximately 29.831 ms difference has not been profiled individually.
The SDK chooses hardware clock dividers, so requested and actual SPI rates
can differ. These six samples establish the learning exercise baseline.
Student confirms the score display looks good after clarification that it
increments every 60 completed frames (about 9.7 seconds at 6.17 FPS).
Student also confirms the green smile and bouncing yellow rectangle display
correctly without trails. Phase 3 practice acceptance checks are complete:
visual output confirmed and measured baseline recorded at 6.17 FPS.
Integration into the production project remains separate work.

## Earlier bring-up and preparation notes

- Phase 3 learning checks completed, including RGB565 size, SPI transfer time,
  and corrected zero-based row-major indexing: (5,3) -> 965 at width 320.
- Prepared Test_Proj phase3_framebuffer: one RAM image, blocking full-screen
  transfer at 10 MHz, score digits, sprite and moving rectangle. Build passed.
- Build inspection: framebuffer symbol=153600 bytes in SRAM (0x200018b0).
  GNU size reports text=35248, data=0, bss=156244 bytes. These are build figures,
  not runtime memory measurements. Host rendering/SPI checks passed.
- At preparation time baseline FPS and hardware validation were pending.
  The requested-clock transfer-only estimate
  is 122.88 ms/frame at 10 MHz, about 8.14 FPS maximum before software overhead.

- Student confirms the deliberately slow ST7789 visual test is working fine
  (100 kHz SPI, 500 ms frame pause, one-pixel motion). No measured FPS or
  explicit border/colour mapping measurements supplied. Next learning step:
  Phase 3 framebuffer/rendering exercise in Test_Proj before production use.

- Student requested very slow pixel updates for observation. Set SPI to
  100 kHz, rectangle steps to one pixel per axis, and frame pause to 500 ms.
  Full-screen transfer time of about 12.3 seconds is calculated, not measured.
  Both display targets rebuilt; hardware observations pending.

- Student reports a tearing-like effect during yellow rectangle motion at
  configured 1 MHz SPI. Flicker versus scan tearing has not been distinguished.
  At the student's request increased SPI to 10 MHz and rebuilt both display
  tests. Visual improvement and signal stability await hardware feedback;
  no FPS measurement or refresh synchronization has been performed.

- With the ST7789 test and inversion enabled, student reports the animation
  background appears white instead of commanded black, with incorrect colours.
  Disabled inversion and rebuilt both display targets; corrected colours await
  visual confirmation. RGB/BGR order has not been changed.

- Student identifies the new LCD controller as ST7789. Earlier ILI9341
  assumptions are superseded. Both display test targets now use st7789.c.
- New ST7789 initialization and 320x240 landscape builds compile successfully.
  Full-screen coverage, colour order and inversion await hardware feedback.

# Measurements

Record observed values with the date, firmware version, hardware setup, and test conditions. A successful build is not proof that the hardware works.

## Phase 0: practice hardware verification

- Board confirmed by student on 2026-10-27: Raspberry Pi Pico 2, non-wireless.
- Practice firmware: `Test_Proj/`.
- 2026-10-27: student reports the onboard LED is blinking with the practice firmware. Blink timing has not been measured.
- 2026-10-27: host-side Python serial reader at `/dev/ttyACM0`, 115200 requested baud, captured `Counter: 337`, `Counter: 338`, and `Counter: 339`. USB output verified; exact output interval has not been measured.
- 2026-10-27: student subsequently confirmed seeing both the LED and serial output. Practice timing is now configured to 100 ms per half-blink; the exact flashed revision was not recorded. The actual project scaffold uses 500 ms per half-blink. Its separate UF2 has not yet been tested on hardware.

## Phase 1: shield inputs

- 2026-10-28: student reports that pressing shield button A works in the GP2 exercise (`Test_Proj/src/phase1_gp2.c`) over USB serial. Planned connection: Funduino V1.A D2 to Pico GP2, shared ground, shield in 3.3 V mode. No voltage or response-time measurements supplied.
- 2026-10-29: student reports testing the shield's buttons individually by keeping the Pico connection on GP2 and moving the shield end of the wire. This verifies the tested shield controls through GP2; it does not verify the other planned Pico GPIO connections or simultaneous presses. Separate joystick-click results were not specified.
- Planned Pico inputs GP3–GP7 and GP9, analog axes, and display wiring: not yet verified.

### Joystick observations reported 2026-10-29

Student observed USB output from the raw joystick ADC exercise and reported:

- Centre: `(1099 - 2111)`; clarification pending whether this is an X/Y pair or a changing range.
- Left: `150 - 200`.
- Right: `3750 - 4095`.
- Up: almost 4095.
- Down: `0 - 20`.
- Y reportedly fluctuates less than X.

The directional observations did not include both labelled X/Y values. Resting
X/Y ranges, axis polarity, and calibration remain pending clarification. These
are student-reported raw counts, not measured voltages or verified noise bounds.

### Released-stick sample set, 2026-10-29

Student supplied 15 consecutive X/Y lines after being asked to release the stick:

```text
X=2104 Y=2084
X=2108 Y=2100
X=2096 Y=2090
X=2090 Y=2109
X=2118 Y=2106
X=2089 Y=2084
X=2096 Y=2084
X=2106 Y=2096
X=2114 Y=2111
X=2105 Y=2095
X=2101 Y=2092
X=2102 Y=2117
X=2102 Y=2091
X=2100 Y=2092
X=2090 Y=2085
```

- X: mean 2101.40, minimum 2089, maximum 2118, peak-to-peak span 29 counts.
- Y: mean 2095.73, minimum 2084, maximum 2117, peak-to-peak span 33 counts.
- This sample set shows centres near 2100 and modest variation; it does not show the earlier ambiguous 1099–2111 span.
- Provisional software centres: X=2101, Y=2096. An initial per-axis dead-zone half-width of 50 counts would contain all these resting samples. This is a proposed starting setting, not hardware-validated calibration or a bound on future noise.

## Display power/backlight test, 2026-10-29

- Module identified by student: SmartElex 3.2-inch 240 x 320 TFT, purchase link `https://robu.in/product/smartelex-3-2-tft-display-240x320/`; matching manual specifies ILI9341.
- Student confirms BK_LIGHT is present, correcting the earlier report that it was absent.
- Student reports connecting display VCC, LEDA, and BK_LIGHT to Arduino 5V with shared ground, but observes no visible illumination.
- Arduino model, measured supply voltages at the connected display pins, current draw, and exact grounding arrangement have not been supplied. This observation does not establish a failed module or the cause of the missing backlight.
- Follow-up: student subsequently reports visible illumination. The backlight power test is now visually successful; voltage measurements and SPI communication have not been reported. Illumination alone does not verify the display controller's communication or initialization.
- Module-side signal check: student reports LCD_RST at 0 V and the other four requested signal pins (LCD_CS, LCD_RS, LCD_MOSI, LCD_WR) at approximately 1.5 V in DC voltage mode. Requested conditions were module powered and Pico signals disconnected; actual probe reference and disconnection conditions remain to be confirmed. These idle readings do not establish logic thresholds, driven logic levels, or signal compatibility. Display VCC voltage and IM0–IM2 connections have not been reported.
- Follow-up clarification: student confirms all five signal pins were disconnected from Pico/Arduino GPIOs and the meter reference was display GND; display VCC measured approximately 5 V relative to display GND. IM0, IM1, and IM2 were all disconnected during those measurements. The idle input readings do not establish a fault or valid logic state.
- Student subsequently reports the backlight test works after the proposed change to Pico VBUS power. No new measured voltages or mode-strap readings were supplied. Display communication remains untested.
- Student requests skipping the standalone GPIO-jumper test and proceeding directly to a visual test. The `phase2_display` practice target builds for RP2350; simulated SPI checks validate address bytes, RGB565 byte order, clipped rectangles, and full-screen transfer length. No visual result, FPS, or module communication success has been observed yet. SPI is configured to 1 MHz; this is a configured value, not a measured clock.
# Replacement display report — 2026-10-30

- Student reports the landscape change still does not cover the 320-pixel
  range. Prepared `phase2_strip_test`: blue X=0..239 reference, then yellow
  and black X=240..319 strip, each held 3 seconds. Result pending; a blank
  strip alone cannot establish a panel fault while controller identity and
  addressing remain unverified.

- Student reports visible output occupies a square with an unused strip
  beside it. Chip marking reported as "xpt2048"; exact marking unconfirmed.
  Prepared landscape diagnostic (320x240, MADCTL=0x28); hardware result pending.

- Student changed to a module marked "2.8 TFT 240*320 V1.1 ROHS" and reports
  connections for MOSI, SCK, DC, RESET, CS, GND and VCC are working.
- No successful image, controller identity, or supply voltage has been
  reported for this replacement. The practice firmware assumes ILI9341 and
  retains the 240x320 portrait dimensions.
