# Pico Arcade Console — project guide

This is a graded learning project. Explain embedded concepts as implementation
progresses so the student can understand and present the code. The student
prefers a `/** ... */` documentation block immediately above every C function
definition and a short matching summary above public declarations. Include
purpose, relevant units/ranges, return value, and hardware/RAM effects when
useful. Keep explanations proportional and update them when behavior changes.

## Workspace and target

- The current folder is the actual production project.
- `Test_Proj/` is the separate learning sandbox; do not build production code
  by pulling in its source files.
- Board: Raspberry Pi Pico 2, RP2350, non-wireless; CMake target `pico2`.
- Display: 2.8-inch 240x320 ST7789, used in landscape 320x240 RGB565.
- Controls: two Funduino/Nokla 5110 Joystick Shield V1.A units. P2 X/Y use a
  CD4066BE analog switch to share ADC2. Exact connections are in `docs/wiring.md`.
- Keep pin numbers and tuning values in `include/config.h`.
- GPIO/ADC are 3.3 V only. Do not connect 5 V signals directly to Pico pins.

## Current product scope

Build a small, USB-powered handheld game console with a launcher, instructions,
pause/results screens, diagnostics, and three playable games:

1. Snake: grid movement with shield buttons, food, growth, score, wall/self
   collision, restart, and a USB developer assist that is explicit and reversible.
2. Pong: two players, each with two-axis paddle control on their own court half;
   impact position and paddle motion affect returns; first to seven wins.
3. Soccer Pool: turn-based shots, five disks per player, ball and disk physics,
   selection/power, goals, and first to three wins.

Treat `docs/architecture.md` and `docs/wiring.md` as the current software and
hardware references. Keep `docs/measurements.md` as an append-only record of
student-reported physical measurements and clearly distinguish those from
calculated or build-time values.

## Architecture and learning approach

The integrated implementation is single-core and uses a blocking SPI frame
transfer. The main loop samples/debounces controls, processes a bounded amount
of USB input, updates simulation at 120 Hz with bounded catch-up, renders to a
single SRAM framebuffer, and presents it on the ST7789. Game rules consume an
input snapshot and do not access GPIO. DMA, multicore rendering, sound, flash
score storage, and sleep are future work, not acceptance criteria for this
first integrated build.

Teach each new embedded concept shortly before using it. For major future
phases, describe the goal and concepts, ask a few check questions, and wait for
the student's response before coding. Do not repeat that gate for implementation
the student has explicitly authorized. Explain fixed timestep, debouncing,
ADC sampling/mux settling, collision response, RGB565/framebuffer RAM, and SPI
transfer cost when relevant.

## Build and checks

Production firmware:

```sh
cmake -S . -B build -DPICO_BOARD=pico2
cmake --build build -j2
```

This generates `build/game_console.uf2`. Host-side rules and UI checks:

```sh
cmake -S . -B build/host -DCONSOLE_HOST_ONLY=ON
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

Do not claim hardware success from compilation or host checks. After a firmware
change, give the student concise steps to flash, open USB serial, exercise the
controls/display, and report back. Do not flash or operate hardware without the
student's request. The student exits `miniterm` with Ctrl+].

## Code and project hygiene

- Keep modules small and readable, with a file header and function docs.
- Keep hardware access at the platform boundary and game rules host-testable.
- Avoid third-party dependencies unless the student asks.
- Do not commit or tag work that has not passed its stated acceptance checks;
  never include unrelated `Test_Proj/` changes in a production commit.
- No phase is accepted until its test outcome is observed and recorded. Builds,
  host checks, and physical tests must be reported separately.

## Done for the first integrated build

- Three games and launcher/rules/pause/results/diagnostics compile for Pico 2.
- Host checks cover portable input, parser, physics, game rules, screens, and
  metrics.
- Student flashes `build/game_console.uf2` and confirms display, both controls,
  game interactions, and USB diagnostics on the physical console.
- Record the observed hardware outcome and any fixes in `docs/measurements.md`.
