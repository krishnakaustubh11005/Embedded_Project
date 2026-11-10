# Learning sandbox

Use this folder for practice programs, small hardware experiments, and learning notes for the Pico 2 project.

The actual Pong console project lives in the parent folder, `Emb_Project/`, with its own source, build configuration, and documentation. Keep practice code here separate from that implementation.

Follow `../Agents.md`: learn each phase's concepts, answer the check questions, then write code. Do not treat an experiment as a completed project phase.

## Phase 0: blink and USB serial

For the confirmed non-wireless Raspberry Pi Pico 2. Run these commands from `Emb_Project/`:

```sh
export PICO_SDK_PATH=/home/krishna/Desktop/Embedded/tools/pico-sdk
cmake -S Test_Proj -B Test_Proj/build -DPICO_BOARD=pico2 \
  -Dpicotool_DIR=/home/krishna/Desktop/Embedded/project_test/build/_deps/picotool
cmake --build Test_Proj/build -j2
```

`CMakeLists.txt` describes the build, `include/config.h` holds the LED and timing settings, and `src/main.c` runs the blink and counter loop.

The explicit `picotool_DIR` selects an existing complete picotool 2.3.0 installation on this computer. The default CMake search found incomplete package version information. This path is machine-specific and must be updated if that installation moves.

Hold BOOTSEL while connecting the Pico over a USB data cable, then release it. Copy `build/phase0_blink.uf2` to the Pico's USB drive. The board should reboot automatically.

The LED on and off durations each come from `BLINK_HALF_PERIOD_MS` in `include/config.h`. At 500 ms, the counter advances about once per second; at the current practice setting of 100 ms, it advances about five times per second. Open the Pico USB serial port (usually `/dev/ttyACM0` on Linux) in a serial monitor. If the monitor requests a baud rate, use 115200; USB serial does not use it to set a physical UART speed. Expect `Counter: 0`, `Counter: 1`, and so on. Early lines can be missed while the computer connects.

If the USB drive does not appear, check the data cable and BOOTSEL procedure. If the counter is missing, check which `/dev/ttyACM*` device appeared and whether the monitor has permission to open it.

Report the LED behavior and a few counter lines before we mark the hardware check as passed.

## Phase 1: read shield button A on GP2

`src/phase1_gp2.c` is a separate practice program. It reads GPIO rather than driving it. An internal pull-up holds the input high when released; pressing the shield's active-low button connects it to ground. `gpio_get()` samples the level. The program polls every 100 ms and prints raw levels; it does not debounce or count presses. USB output adds some overhead to the sampling interval.

With power disconnected, wire shield D2 to Pico GP2 (physical pin 4), shield GND to Pico GND (physical pin 38), and shield 3V3 to Pico 3V3 OUT (physical pin 36). Set the shield voltage selector to 3.3V and leave its 5V connection unused. These instructions are for the identified Funduino V1.A shield; do not confuse its Nokia 5110 connector with the button header.

From `Emb_Project/`, configure using the Phase 0 command above, then build this target:

```sh
cmake --build Test_Proj/build --target phase1_gp2 -j2
```

Exit any serial monitor, connect in BOOTSEL mode, then run:

```sh
picotool load Test_Proj/build/phase1_gp2.uf2
picotool reboot
python3 -m serial.tools.miniterm /dev/ttyACM0 115200
```

Hold A for at least a second: expect repeated `GP2: 0 (0=pressed, 1=released)` lines. Release it: expect repeated `GP2: 1 (0=pressed, 1=released)` lines. Check both transitions several times. The LED is not used by this program.

If always high, check D2-to-GP2 and common ground. If always low, disconnect power and check for a short to ground or the wrong header pin. Report observations before expanding to the other inputs.

Learning experiment after the basic test: predict what happens to brief presses when `BUTTON_POLL_INTERVAL_MS` changes from 100 to 500, then rebuild, flash, and compare. A longer blocking delay makes input less responsive and can miss a complete press-release between samples. This illustrates polling and input latency; it is not a debounce implementation.

## Serial monitor with automatic reconnection

Instead of miniterm, run this from `Emb_Project/`:

```sh
python3 Test_Proj/serial_monitor.py
```

Or specify the initial port and baud rate:

```sh
python3 Test_Proj/serial_monitor.py /dev/ttyACM0 115200
```

This receive-only monitor uses the already installed `pyserial` package. It prints `Waiting for Pico...` while the board is absent, displays incoming firmware output when connected, and retries after USB disconnects. A quiet but connected board simply produces no text. Before its first connection it prefers the requested port, or selects a single detected Pico serial port. If several Pico ports exist, specify the desired port. Once connected, it remembers the board's USB serial number when provided, allowing reconnection after its port name changes. Without a USB serial number, it prefers the last port name or falls back to a single detected Pico; it cannot guarantee board identity in that case. BOOTSEL mode has no firmware serial output; the monitor waits until the program runs again.

Use Ctrl+C to exit. Close other serial monitors before starting this one, and stop it before flashing. This script reads text only; it does not send typed commands to the Pico or recover text transmitted while disconnected.

## Phase 1: all shield buttons

Follow the button wiring table in `../docs/wiring.md`, with USB disconnected
while wiring. `src/phase1_buttons.c` uses `setup_button(pin)` as a helper:
each call supplies a different GPIO number to the same input-and-pull-up setup.
The function returns no value (`void`). Each `%d` in the print format corresponds
to one GPIO reading in the following argument list.

Configure using the Phase 0 CMake command above, then build:

```sh
cmake --build Test_Proj/build --target phase1_buttons -j2
```

Close the monitor, connect in BOOTSEL mode, and run:

```sh
picotool load Test_Proj/build/phase1_buttons.uf2
picotool reboot
python3 Test_Proj/serial_monitor.py
```

With all controls released expect `A=1 B=1 C=1 D=1 E=1 F=1 K=1`.
Press and hold each control separately: only its value should become 0.
K means pressing the joystick down, not moving it sideways. Also hold A and B
together to check both can be read low in the same printed line. These are raw
levels, with no debounce. Analog axes are not used by this program.

## Phase 1: joystick ADC readings

With USB disconnected, wire shield A0 to GP26 (physical pin 31) and A1 to GP27
(physical pin 32). Keep the existing 3.3 V supply, shared ground, and shield
selector at 3.3 V. GPIO 26 is ADC channel 0; GPIO 27 is ADC channel 1.

`src/phase1_joystick.c` initializes the ADC, prepares both pins for analog input,
then selects and reads each channel. `adc_read()` starts a conversion and waits
for the result. Each result is stored in an `unsigned int` and printed with `%u`.
The CMake target links `hardware_adc` to provide the ADC implementation. No
filtering, centre calibration, dead zone, or direction mapping is applied yet.

Configure with the CMake command above, then build:

```sh
cmake --build Test_Proj/build --target phase1_joystick -j2
```

Close the monitor and connect the Pico in BOOTSEL mode:

```sh
picotool load Test_Proj/build/phase1_joystick.uf2
picotool reboot
python3 Test_Proj/serial_monitor.py
```

Expect lines like `X=2048 Y=2048`, with actual values depending on the hardware.
Readings have 12-bit resolution (0–4095). Hold the stick at rest for several
lines, then at left, right, up, and down. Record both readings for each position.
Observe which value changes and whether it increases or decreases; do not
assume direction polarity from the axis labels. Endpoints need not reach exactly
0 or 4095, and centre need not be exactly 2048.

If an axis stays at one extreme or fluctuates wildly, disconnect power and
check its wiring, common ground, and voltage selection. Disconnected analog
inputs can float. This program does not use the buttons or blink the LED.

## Phase 1: display signal jumper check

`src/phase1_display_pins.c` reuses GPIO output and a helper function to test
GP17, GP18, GP19, GP20, and GP21 one at a time. Each holds low for five seconds,
then high for five seconds. It prints the current GPIO and expected voltage.
This is a slow wiring test, not SPI communication or display initialization.

With USB disconnected, attach jumper wires to those Pico GPIOs, but leave their
display ends unplugged and insulated from each other. Label them according to
the proposed mapping in `../docs/wiring.md`. Do not attach this test's outputs
to the display. The display's existing power/backlight wiring may remain; it
does not participate in this signal test.

Configure using the command above, then build:

```sh
cmake --build Test_Proj/build --target phase1_display_pins -j2
```

Close the serial monitor, connect in BOOTSEL mode, then run:

```sh
picotool load Test_Proj/build/phase1_display_pins.uf2
picotool reboot
python3 Test_Proj/serial_monitor.py
```

In DC voltage mode, black probe on Pico GND and red probe on the free end of
the jumper under test, verify its low and high voltages against the printed
status. Wait for another cycle if needed; all five tests repeat. Report the
low/high readings for each GPIO. Do not infer module input compatibility or
successful SPI communication from this standalone output test. Disconnect USB
before moving any wires or later connecting the display signal ends.

## Standalone display animation

The `phase2_display` target cycles through red, green, and blue, then animates a
yellow rectangle. It needs no joystick or game logic. See
[`../docs/display-test.md`](../docs/display-test.md) for wiring, build/flash
commands, the code explanation, and observations to report.
# Phase 3 framebuffer exercise

The [button-controlled Snake exercise](snake.md) adds game rules, USB developer
assistance and separate control/render/transfer timing in `phase4_snake`.

The [FPS benchmark](fps_benchmark.md) compares direct 16-bit transfers at
several clocks against the saved 6.17 FPS baseline.

See [the framebuffer walkthrough](phase3_framebuffer.md) for the new
`phase3_framebuffer` target, flashing, memory layout, and FPS measurements.
