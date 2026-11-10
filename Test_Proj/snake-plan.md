# Snake input-and-timing exercise

Current requested controls: A/GP2 up, B/GP3 right, C/GP4 down, D/GP5 left.

Requested 2026-10-31, before advancing the Soccer Pool project. Implementation
was initially pending learn-first checks. After explanation of classic movement
and reversal rules, the student confirmed using four shield buttons connected
simultaneously to GP2..GP5. Implementation is prepared; see snake.md for use
and remaining hardware checks.

## Purpose and hardware

Build a separate `phase4_snake` practice target using the confirmed Pico 2,
ST7789 320x240 landscape display and buttons A/B/C/D on GP2/3/4/5. Reuse the single
SRAM framebuffer and 16-bit blocking SPI path. Use the visually successful
125 MHz system/peripheral source with requested SPI=62.5 MHz. Document every
function. The visualization website remains deferred.

The observed maximum was around 42 FPS. A 43 FPS target allows 23.256 ms per
frame, but neither that rate nor every input response is guaranteed. Report
measured input, update, draw, transfer and whole-loop times, maximum sampling
gap, and frames exceeding the target budget. A scheduled snake movement is
separate from CPU execution time and input-to-visible response latency.

## Proposed game

- Snake on a cell grid, ball-shaped food, growth and a score.
- Four active-low direction buttons with internal pull-ups and timer debounce.
- Reject direct reversal relative to the actual movement direction. Limit
  accepted queued turns so several samples cannot cause a reversal before a
  movement tick.
- Sample controls each main-loop iteration; use fixed elapsed-time deadlines
  for movement (initially eight cells/second) instead of one move per frame.
- Wall/body collisions end a normal game. Food must spawn on a free cell;
  a full board is a win rather than an infinite food-placement loop.
- Start/reset and pause/resume commands over USB; any new direction-button
  press after game over restarts. GP2 is up, not a pause button.
- Input reading, movement update, framebuffer rendering, SPI sending and USB
  parsing remain short separate functions with clear state ownership.

## Developer assistance

Recognize a complete USB serial line `DEV 2350` to enable a developer-assist
flag. `DEV OFF` disables it. This is a deliberate game cheat command, not
identity detection. Explain in the game/serial output when it is enabled.

Proposed assistance: slower movement (four cells/second) and wrapping at screen
edges instead of wall death. Body collisions still end the game. Keep mode
selection in RAM, with no flash persistence. A visible DEV indication prevents
confusing assisted results with normal play. Restart can preserve the current
mode; reboot returns to normal.

USB parsing must use nonblocking character reads, bounded per-frame work and a
bounded line buffer. Handle CR/LF and overflow; never stall the game waiting
for someone to finish typing the secret code. Provide clear serial responses.

## Learning prerequisites and scope

Explain stable-time button debounce, polling versus short interrupt handlers,
the purpose/limits of volatile, fixed-timestep updates, simple grid collision
checks, and nonblocking serial command parsing. Use polling and ordinary state
in one core initially; interrupts, DMA, flash high scores and full Soccer Pool
physics are separate later work. This exercise does not complete production
Phase 4.

## Validation after implementation

Host-check movement, opposite-turn rejection, food growth/placement, body/wall
collisions, pause/restart, developer wrapping, command parsing and bounds.
Build a Pico 2 UF2. Have the student check joystick direction, normal/assisted
play and serial responses on hardware, then supply timing samples. Record
whether the 23.256 ms frame target is actually met and distinguish sampling
latency, debounce delay, game tick rate and full-frame delivery rate.
