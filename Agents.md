# PIO Pong: RP2350 handheld game console (EE4500 project)

You are helping a student build this project. It is graded, so the student must understand and be able to explain every line. Read this whole file before doing anything.

## Workspace organization

- `Test_Proj/` is the learning sandbox: keep practice programs, small experiments, and learning notes there.
- The current folder (`Emb_Project/`) is the actual console project root. Keep its production source, build configuration, and project documentation here, following section 7.
- Learning exercises do not count as completed project phases. Follow the learn-first checks below before implementing each phase, and move practice ideas into the actual project only when understood and needed.

## 1. Goal

Build a self-contained, USB-powered handheld game console on the Raspberry Pi Pico 2 (RP2350) that plays a Pong variant with sound, persistent high scores and good input response. Deliverables: working hardware, schematic, commented source on GitHub, a report with measurements (FPS, memory, energy), and a demo video.

Grading weights (so you know where effort goes): working hardware with ease of play 50%, code quality 15%, advanced features 15%, report 10%, polish 10%. Time budget: about 14 days.

## 2. Hardware (confirm before coding)

- Board: Raspberry Pi Pico 2 (RP2350). Build target is `PICO_BOARD=pico2`, Arm cores.
- Display: SPI TFT, ST7789, 240x320, RGB565. **Ask the student to confirm the exact module and resolution first.** Keep width, height and SPI speed as `#define`s in one config header.
- Buttons: 6 tactile, active low with internal pull-ups.
- Audio: PWM/PIO output, RC filter, class-D amp, 8 ohm speaker.
- Optional: lid/sleep switch, UART link jack, INA219 current sensor.

### Pin map (all 3.3 V)

| Signal | GPIO |
|---|---|
| Link TX (UART0) | GP0 |
| Link RX (UART0) | GP1 |
| Button UP | GP2 |
| Button DOWN | GP3 |
| Button A | GP4 |
| Button B | GP5 |
| Button START | GP6 |
| Button SELECT | GP7 |
| Lid/sleep switch | GP8 |
| Audio out | GP15 |
| Display MISO (unused) | GP16 |
| Display CS | GP17 |
| Display SCK | GP18 |
| Display MOSI | GP19 |
| Display DC | GP20 |
| Display RST | GP21 |
| Backlight (PWM) | GP22 |
| Supply sense (optional ADC) | GP26 |

Put every pin in `config.h`. Never scatter pin numbers through the code.

## 3. Game spec

- Modes: 1 player vs a simple scripted CPU paddle (tracking with reaction delay and error, no machine learning), and 2 players over the link cable (stretch goal).
- Physics: ball angle depends on where it hits the paddle, speed ramps up each rally, optional spin.
- Visuals: paddles, ball, court, score digits, all drawn into an RGB565 framebuffer.
- Persistence: top 5 scores in flash with a CRC-checked record.
- Polish: title screen, pause, screen shake on a point.

## 4. Architecture rules

- Core 0: input, game state and physics at a fixed timestep, flash writes.
- Core 1: rendering into the framebuffer and display transfer.
- Phase-by-phase evolution: single buffer + blocking SPI first, then DMA with double buffering. Keep the early version working and tagged in git so the FPS comparison is possible.
- Shared state between cores must be safe (a simple struct with a clear writer and reader, or the inter-core FIFO). Explain the race risk in comments.
- Use the Pico SDK APIs (`hardware_spi`, `hardware_dma`, `hardware_pwm`, `hardware_pio`, `pico_multicore`, `hardware_flash`). Do not add third-party libraries without asking.
- Write the ST7789 driver as a small file the student can read: init sequence, set window, push pixels. Do not pull in a large graphics library.

## 5. How you must work

1. Work **one phase at a time** (section 6). At the start of each phase, state in a few lines what you will build and which concepts it uses. Stop at the end of the phase and wait.
2. After each phase, give a short "what to test" list. You cannot see the hardware, so the student will report serial output, photos or measurements back. Do not assume a result worked.
3. Keep code simple and readable. Short functions, clear names, a comment on every non-obvious line, and a header comment per file saying what it does. Prefer clarity over cleverness.
4. Explain each new concept briefly when it first appears (what it is and why it is needed), in plain language.
5. If something could be wrong (SPI mode, byte order, wiring), say what to check instead of guessing silently.
6. Commit after each phase with a clear message and tag the commit (`phase-3-single-buffer`, etc.).
7. Never invent register values or datasheet facts. If unsure, say so and tell the student where to check.
8. Keep a `docs/measurements.md` file and append every measurement as it is taken, with the date and conditions.
9. **Learn first, then build, in parallel.** Every phase has a "Learn first" list. Before writing any code for a phase: (a) teach those topics in order, each in a few lines (what it is, why this project needs it, a tiny example), (b) ask the student 2-3 short check questions and wait for answers, correcting gently if wrong, (c) only then start coding. During the phase, introduce each further concept right before the step that uses it, so learning and building stay side by side. Never dump all theory at once, and never write code the student has not been prepared for. If the student already knows a topic, skip it quickly.

## 6. Phases and acceptance tests

**Phase 0: Setup (Day 1)**
- **Learn first (teach before coding):** How the Pico SDK build flow works: C source -> CMake -> `.elf` -> `.uf2`, and flashing in BOOTSEL mode; What `PICO_BOARD=pico2` means and why RP2350 differs from RP2040; GPIO output basics (`gpio_init`, `gpio_set_dir`, `gpio_put`); USB serial: enabling `stdio_usb`, `printf`, opening a serial monitor; Git basics: `init`, `add`, `commit`, `.gitignore`.
- Check Pico SDK path, CMake, ARM toolchain. Create the repo structure below with `.gitignore` (ignore `build/`).
- Build and flash blink. Add USB serial `printf`.
- Done when: LED blinks and a counter prints every second.

**Phase 1: Wiring checks (Day 1-2)**
- **Learn first (teach before coding):** GPIO input, internal pull-ups and why buttons are active low; 3.3 V logic, common ground, and checking wires with a multimeter in continuity mode; Reading a pinout and the project pin map; Preview: what switch bounce is (full fix comes in Phase 4).
- Test each button (print its state), then the display wiring by reading pin levels or a simple toggle test.
- Done when: all 6 buttons print correctly, and the wiring matches the pin map.

**Phase 2: Display bring-up (Day 2)**
- **Learn first (teach before coding):** SPI protocol: SCK, MOSI, chip select, clock speed, modes 0-3; The DC pin: command vs data bytes on the ST7789; ST7789 init commands (reset, sleep out, colour mode, orientation, inversion, display on) and how to find them in the datasheet; Setting the draw window (column/row address, memory write); RGB565 colour format and why bytes may need swapping.
- ST7789 driver: reset, init sequence, set window, write pixels. Fill the screen red, green, blue.
- Check byte order and colour inversion if colours look wrong.
- Done when: solid colours appear correctly and a rectangle draws in the right place.

**Phase 3: Framebuffer, sprites, FPS (Day 3-4)**
- **Learn first (teach before coding):** What a framebuffer is and how much RAM it takes (width x height x 2 bytes); Memory layout: flash vs SRAM vs stack, `const` data living in flash, and how to read the linker map; C for graphics: fixed-width types, arrays, structs, bit shifts and masks; Sprites and a bitmap font as arrays of data; Timing: `time_us_64()` and computing FPS.
- One `uint16_t` framebuffer in SRAM. Functions: clear, set pixel, fill rect, draw sprite, draw text/digits with a small bitmap font.
- Push the whole frame over blocking SPI. Add an FPS counter printed once per second.
- Done when: a moving rectangle and score digits animate on screen and the **baseline FPS** is recorded in `docs/measurements.md`.

**Phase 4: Input and game loop (Day 5-6)**
- **Learn first (teach before coding):** Why buttons bounce and two fixes (timer-based sampling, interrupt plus timestamp); Polling vs interrupts, and why ISRs must be short; `volatile`; Fixed-timestep game loops; 2D ball motion, wall and paddle collisions (reflection of velocity); Flash basics: sector erase (4 KB), page program (256 B), why writes are slow and wear matters, and CRC for integrity.
- Debounced buttons (timer-based or interrupt plus timestamp, no missed or doubled presses).
- Fixed-timestep loop, paddle and ball physics, collisions, CPU paddle, score display.
- High scores saved to flash with CRC, with a power-cycle test.
- Done when: a full single-player game is playable and scores survive a reboot.

**Phase 5: Dual core and DMA (Day 7-8)**
- **Learn first (teach before coding):** Multicore: `multicore_launch_core1`, the inter-core FIFO, and what a race condition is; Safe sharing of data between cores (single writer, flags, memory ordering at a high level); DMA: channels, transfer count, data request (DREQ) pacing from SPI, completion interrupt, chaining; Single vs double buffering and why double buffering needs twice the RAM.
- Move rendering to core 1. Add DMA with double buffering for the display push.
- Done when: FPS is re-measured and compared with the Phase 3 baseline in the measurements file.

**Phase 6: Sound and sleep (Day 9-10)**
- **Learn first (teach before coding):** PWM: frequency, duty cycle, wrap value and how a tone is made; PWM audio, RC low-pass filtering, coupling capacitor, amplifier and speaker basics; Optional: what a PIO state machine is (only if using PIO for audio); Low-power modes: sleep vs dormant, wake sources, and what each stops; How to measure current (multimeter or INA219).
- Audio through PWM or PIO with simple chiptune effects (hit, score, menu).
- Sleep after an idle timeout or on the lid switch, wake on button. Measure current if the sensor is available.
- Decision point: keep or drop the link cable.

**Phase 7: Polish (Day 11-12)**
- **Learn first (teach before coding):** Simple state machines for title, play and pause screens; Spin physics: turning paddle hit position into an exit angle; Float vs fixed-point on the Cortex-M33 (what the single-precision FPU gives you).
- Spin physics, screen shake, title and pause screens, enclosure fit check, button feel.

**Phase 8: Report material (Day 13-14)**
- **Learn first (teach before coding):** How to measure memory use (`size`, linker map), FPS, current and input latency (GPIO toggle on a scope); How to structure a short technical report: setup, method, results, discussion.
- Measurements: FPS (single vs DMA double buffer, audio on/off), memory (flash and SRAM from the linker map), current in active/idle/sleep, input latency (button to pixel).
- Write README with build steps, wiring table and architecture diagram description.

**Stretch (only if on schedule): link cable**
- **Learn first (teach before coding):** UART basics: baud rate, framing, TX/RX crossing; Packet framing and checksums, and a host/client protocol for ball physics.
- UART with framed packets and a checksum. One console is the host for ball physics. The jack wiring on one console swaps TX and RX; do not assume a crossover cable.

## 7. Repo structure

```
/src        main.c, game.c, render.c, input.c, audio.c, storage.c, st7789.c
/include    config.h and module headers
/docs       measurements.md, wiring.md, architecture.md
CMakeLists.txt
README.md
```

## 8. Things to watch for

- ST7789 often needs the byte order swapped per pixel and the display-inversion command; wrong colours usually mean this.
- If the display shows garbage, try SPI mode 0 then mode 3, and lower the clock to about 10 MHz before raising it.
- Full-frame SPI at 40 MHz takes roughly 30 ms (so about 25-30 FPS is the expected baseline); DMA and partial updates are what improve it.
- The Pico GPIOs are 3.3 V only; never connect 5 V signals directly.
- Flash writes must not run while the other core executes code from flash unless the SDK safe-execution calls are used.

## 9. Definition of done

Everything in the Phase 0 to 6 acceptance tests passes, measurements are recorded, the code is commented and on GitHub, and the student can explain the architecture without notes.
