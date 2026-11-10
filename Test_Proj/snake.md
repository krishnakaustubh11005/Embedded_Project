# Snake: button controls, developer mode and timing

This is a separate learning exercise in Test_Proj, before production Soccer
Pool integration. It uses the confirmed four shield buttons, the ST7789 and
one RGB565 framebuffer, with 125 MHz CPU/peripheral and 62.5 MHz requested SPI.
It is not a completed production Phase 4 or a claim of guaranteed 43 FPS.

## Controls and rules

| Shield button | Pico GPIO | Direction |
|---|---|---|
| A / D2 | GP2 | Up |
| B / D3 | GP3 | Right |
| C / D4 | GP4 | Down |
| D / D5 | GP5 | Left |

All four connections were confirmed together by the student. Inputs use
internal pull-ups: pressed is low. Joystick axes are not read in this target.
Buttons request direction changes; classic Snake keeps moving automatically.
Normal speed is eight cells/second, independent of render FPS. A direct
reverse is ignored. At most one turn is queued for each movement step;
simultaneous new presses have A/B/C/D priority.

Eat red ball-shaped food to grow and gain one point. The head is yellow and
the body green. In normal mode, walls or your body end the game. A new press
of any direction button after death/win restarts. A button held from before
death must be released and pressed again. Filling all cells wins.

The screen is 320x240 landscape: a 40-pixel header and a 40x25 grid of 8-pixel
cells. The rules state has capacity for 1040 cells but this layout uses 1000.
Score and mode are in RAM; reboot resets both, restart clears score and keeps
the current mode.

## Build and flash

Run from Emb_Project:

```sh
cmake -S Test_Proj -B Test_Proj/build -DPICO_BOARD=pico2
cmake --build Test_Proj/build --target phase4_snake -j2
```

Close the serial monitor and connect in BOOTSEL mode:

```sh
picotool load Test_Proj/build/phase4_snake.uf2
picotool reboot
python3 -m serial.tools.miniterm /dev/ttyACM0 115200 --echo --eol LF
```

Use a bidirectional monitor like miniterm for commands; the existing
`serial_monitor.py` is read-only. Type a command, then Enter. Quit miniterm
with Ctrl+]. Use the actual device path if USB enumeration changes it.

## Developer and other commands

| Exact line | Result |
|---|---|
| `DEV 2350` | Enable assistance: four cells/second and wall wrapping |
| `DEV OFF` | Restore eight cells/second and normal wall collisions |
| `PAUSE` | Pause a playing game |
| `RESUME` | Resume a paused game |
| `RESTART` | Restart while retaining the selected mode |
| `HELP` | Print the available commands |

A cyan DEV label and cyan playfield border show assistance is active. Body
collisions still end play. Enabling DEV after death does not revive the snake;
restart with a button or RESTART. The CPU matches a command string and sets a
flag; it does not identify the person playing.

USB reads are nonblocking, with at most 32 characters processed per frame.
Commands are case-sensitive. CR, LF and CRLF are accepted; backspace works.
Overlong lines are discarded rather than interpreted as truncated commands.

## Follow the code

- `src/snake.c`: pure game rules and RAM state. `snake_request_turn()` buffers
  a legal turn; `snake_step()` advances one cell, handles collisions and food,
  and updates the occupancy array. A moving tail cell may be entered safely.
- `src/snake_input.c`: four timer-debounced button states and a bounded command
  parser. These functions take samples/characters and never perform hardware I/O.
- `src/snake_ui.c`: original small letter glyphs, four score digits, snake and
  integer-circle food drawing into the existing framebuffer.
- `src/phase4_snake.c`: hardware setup and the measured loop: sample/debounce,
  parse commands, queue turns, perform due movement ticks, draw, send, report.
- `src/st7789.c`: existing 16-bit blocking pixel transfer. No DMA/second core.

Every new function has a documentation block describing purpose, arguments,
return values and important state/hardware effects. Static storage keeps large
arrays off the function-call stack. There are no heap allocations or flash writes.

## What the timing report means

The 43 FPS target is 1000000/43 = about 23255.8 us per frame. The integer
comparison uses 23255 us. The game runs unpaced so observed FPS shows its cost.

| Serial field | Meaning |
|---|---|
| FPS | Completed full-frame transfers divided by wall time, including prior periodic reporting |
| input | Average GPIO sampling, debounce and USB parsing/command time in us/frame |
| logic | Average button routing and scheduled game updates in us/frame |
| draw | Average framebuffer composition time in us/frame |
| SPI | Average blocking full-frame transfer time in us/frame |
| controls_max | Maximum combined input + logic execution time in this report window |
| frame_max | Maximum input + logic + draw + SPI time, excluding periodic report printing |
| input_gap_max | Maximum time between button polls, including reporting overhead |
| over_budget | Number of measured frames longer than 23255 us |
| gap_over | Number of sampling gaps longer than 23255 us |
| dropped_ticks | Overdue movement ticks skipped after the four-step catch-up cap |
| SPI clock | Actual hardware divider-selected SPI clock in Hz |
| DEV | 1 when assistance is active, 0 in normal mode |

Averages and maxima reset each reporting interval. Button history and the
last polling timestamp persist. Report output itself is outside measured
stage times but is included in subsequent FPS and input-gap measurements.
Command responses are inside input time.

Short control execution does not prove a sub-frame physical response. A
press must first be sampled, pass 20 ms observed-stability debounce, wait for
a scheduled movement tick (125 ms normal or 250 ms DEV), and reach the panel
through SPI. Polling once per blocking frame can miss a press shorter than
the sampling gap. Debounce adds at least one further polling interval in this
configuration. Do not confuse those delays with CPU calculation time.

If about 42 FPS remains the observed rate, it does not strictly meet a 43 FPS
whole-loop target. The purpose is to measure the margin and identify whether
control work or display transfer limits it. Future DMA can improve concurrent
input handling; this exercise does not implement it.

## Software checks and hardware report

Build the Pico UF2 as above. Host rules/input checks can be run without hardware:

```sh
cc -std=c11 -Wall -Wextra -Werror -ITest_Proj/include \
  Test_Proj/src/snake.c Test_Proj/src/snake_input.c \
  Test_Proj/tests/snake_test.c -o /tmp/snake_test
/tmp/snake_test
```

During preparation, these checks passed with AddressSanitizer/UBSan: legal
movement, reverse/queued-turn rejection, growth, normal wall death, DEV wrap,
body/tail collisions, maximum-capacity win, food placement, 20000-step state
invariants, debounce bounce/hold/release and serial parsing/overflow recovery.
The actual framebuffer renderer also passed host pixel checks and was rendered
to inspect header spacing, labels, body and ball layout.

Hardware game operation and timing remain pending. Report button directions,
food/growth/score, death/restart, PAUSE/RESUME, DEV ON/OFF, and five timing lines
from normal play plus five from assisted play. Include any corrupted pixels or
missed buttons. This exercise does not complete the production game phase.
