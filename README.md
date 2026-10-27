# PIO Pong — Pico 2 console

EE4500 project for the non-wireless Raspberry Pi Pico 2 (RP2350, Arm cores).
The actual firmware lives here. `Test_Proj/` contains separate learning exercises.
Follow `Agents.md` for the phase plan and learn-first workflow.

## Phase 0 build

Run from this folder:

```sh
export PICO_SDK_PATH=/home/krishna/Desktop/Embedded/tools/pico-sdk
cmake -S . -B build -DPICO_BOARD=pico2 \
  -Dpicotool_DIR=/home/krishna/Desktop/Embedded/project_test/build/_deps/picotool
cmake --build build -j2
```

These SDK and picotool paths refer to existing installations on this computer;
update them on another machine. The explicit picotool path selects a complete
2.3.0 CMake package. Build directories are ignored by Git.

## Flash and observe

Connect the board while holding BOOTSEL, then release the button:

```sh
picotool load build/pio_pong.uf2
picotool reboot
python3 -m serial.tools.miniterm /dev/ttyACM0 115200
```

The LED should alternate on and off every 500 ms and the counter should print
approximately once per second. The serial port may have a different number;
use `python3 -m serial.tools.list_ports` to find it. Exit the monitor with Ctrl+].

## Layout

- `src/main.c`: current Phase 0 firmware.
- `include/config.h`: central hardware and timing settings.
- `docs/`: wiring, architecture, and observed measurements.
- `Test_Proj/`: independent practice programs.

Later modules will be added during their corresponding phases.
