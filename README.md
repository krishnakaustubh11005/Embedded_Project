# Pico Arcade Console

Firmware for the non-wireless Raspberry Pi Pico 2 (RP2350), a 2.8-inch
240x320 ST7789 display, and Funduino joystick-shield controls. The firmware
currently includes Snake, two-axis two-player Pong, turn-based Soccer Pool,
and Shade Duel, a two-player silhouette fighting game.
`Test_Proj/` remains the separate learning sandbox; production source is in
`src/` and `include/`.

## Build

Configure with the Pico SDK installed and `PICO_SDK_PATH` set:

```sh
cmake -S . -B build -DPICO_BOARD=pico2
cmake --build build -j2
```

The firmware UF2 is `build/game_console.uf2`. Enter BOOTSEL mode to copy it
with `picotool load build/game_console.uf2`, then reboot with `picotool reboot`.
USB serial uses 115200 baud. Find the port with
`python3 -m serial.tools.list_ports`, then open it with
`python3 -m serial.tools.miniterm PORT 115200` (Ctrl+] exits).

## Controls and menu

Use A/C to move through menus and B to select. In games, START/SELECT
(joystick-click inputs) pause and resume. Snake uses A=up, B=right, C=down,
D=left. Pong uses the two joystick axes for each player's paddle. Soccer Pool
uses A/C to choose a disk, B to shoot, and D to set shot power. Shade Duel
uses the stick to walk/jump/crouch, A punch (forward+A uppercut), B kick
(down+B sweep, airborne B flying kick), C block (back+C roll) and D for the
Brazilian kick, which needs a full energy cell. On the Shade Duel rules
screen, C switches the sound theme between NINJA (synthesized hits) and CLIPS
(the recorded kick/punch sounds). USB commands
include `HELP`, `MENU`, `STATUS`, `CAL`, `MUTE`/`UNMUTE`, `THEME` and `DEV ON`/`DEV OFF`; see
`docs/architecture.md` for details.

## Project notes

- Pin assignments and mux wiring: [docs/wiring.md](docs/wiring.md)
- Software structure and USB commands: [docs/architecture.md](docs/architecture.md)
- Student-reported hardware and timing observations: [docs/measurements.md](docs/measurements.md)
- Learning sequence and collaboration instructions: [Agents.md](Agents.md)

The host-side rule/UI tests can be built with
`cmake -S . -B build/host -DCONSOLE_HOST_ONLY=ON`, then
`cmake --build build/host` and `ctest --test-dir build/host --output-on-failure`.
