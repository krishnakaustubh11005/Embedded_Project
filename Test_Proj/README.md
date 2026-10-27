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
