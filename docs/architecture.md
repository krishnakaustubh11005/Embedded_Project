# Firmware architecture

The Pico runs a single-core loop: sample and debounce inputs, update the
selected game's state at a fixed 120 Hz step, render into one RGB565 SRAM
framebuffer, and send that framebuffer to the ST7789 with blocking SPI. This
simple ownership model avoids cross-core races while the games and controls
are being validated. The SPI transfer currently dominates frame time; DMA and
second-core rendering remain later optimization work.

```text
USB serial ──> command parser ─┐
ADC/buttons ─> input snapshot ─┼─> app state + game rules ─> framebuffer
                              └───────────────────────────────────────┐
ST7789 <──────────────────── blocking SPI <───────────────────────────┘
```

`src/platform/board.c` owns Pico GPIO, ADC, clock, and display setup.
`src/platform/input.c` turns raw values into calibrated/debounced snapshots.
`src/app/` selects screens and games. Each game has rule/update and renderer
files under `src/games/`. `src/graphics/` owns the RGB565 framebuffer and
primitive drawing functions. `src/diagnostics/metrics.c` reports loop timing.

## Screen and game flow

The application provides a launcher (a 2x2 tile grid plus a diagnostics
entry), static game instructions, four game modes, pause/resume, result
screens, and diagnostics. Snake moves on a grid
with button controls. Pong has two independently controlled paddles, each
moving vertically and horizontally within its half; a hit reflects the ball
with an angle influenced by impact position and paddle movement. Soccer Pool
has five disks per side and one ball; players select a disk and take turns
shooting until pieces settle, with a goal limit of three.

Shade Duel is a two-player silhouette fighting game (best of three 60-second
rounds). Fighters are not bitmaps: `src/games/fighter/fighter_pose.c` stores
each move as a few keyframes of joint angles, blends them with a smoothstep
ease, and uses forward kinematics to place the joints. The lowest body point is
snapped to the floor, so crouches, sweeps and knockdowns stay grounded. Limbs
are drawn as filled capsules (`gfx_capsule`). The stick walks, jumps (up) and
crouches (down). Buttons are A punch / forward+A uppercut, B kick / down+B
sweep / airborne B flying kick, C hold to block (down+C low block, back+C dodge
roll), and D Brazilian kick, which costs one of two energy cells. Energy fills
when you land, take or block hits. Attack heights matter: high attacks miss
crouching fighters, standing block stops high/mid, crouch block stops low/mid.
Hits are detected from the striking fist or foot of the animated skeleton
against the opponent's body box. Presses are buffered for 150 ms.

All screens share one palette and rounded-card style from `include/graphics/gfx.h`
(P1 blue, P2 red). Each `GameSpec` supplies a tile colour, a launcher icon
and a winner callback that colours the results card.

Each game consumes an `InputSnapshot` and does not access hardware directly.
This keeps hardware sampling separate from the game rules and makes the rules
host-testable. Simulation time is accumulated in fixed steps, with a bounded
catch-up count so a slow display frame cannot cause an unbounded update loop.

## USB diagnostics and developer mode

USB stdio is non-blocking in the main loop. The parser consumes a bounded
number of available characters per frame. `HELP` lists commands; `STATUS`
prints screen/game and timing information; `MENU` returns to the launcher;
`CAL` recalibrates controller centers; `MUTE`/`UNMUTE` silence the speaker;
`THEME` switches the Shade Duel sound theme; and `DEV ON`/`DEV OFF` toggles the
separate developer assist in Snake. Developer mode does not bypass the other
games' rules. Commands are processed as text lines over USB CDC, not by
`picotool` after firmware has booted.

## Audio

`src/audio/sound.c` is portable: games call `sound_play(SND_...)` and
`sound_music(...)`, which only push one byte into a 16-entry ring queue. The
Pico layer `src/platform/audio_pwm.c` drives GP22 with 10-bit PWM (122 kHz
carrier). A DMA pacing timer copies one level per 1/22050 s from RAM into the
PWM compare register. Two chained DMA channels ping-pong between two 256-sample
buffers; when one finishes, its interrupt drains the queue and synthesizes the
next 11.6 ms. Because DMA, not the CPU, delivers samples, the blocking display
transfer does not interrupt sound.

Effects are tables of tones (waveform, pitch slide, volume slide, duration),
so each costs bytes rather than kilobytes. Five effect voices and three music
voices (koto-style lead, bass, taiko drums) are mixed. Shade Duel's loop is
an original 8-bar tune in the Japanese In scale (D Eb G A Bb) at 132 BPM.
The CLIPS theme swaps punch, kick and heavy hits for 8-bit PCM made from
`kick.mp4`, `punch1.mp4` and `punch2.mp4` by `tools/audio_to_c.py` (about
29 KB of flash). Re-run that script after replacing the recordings.

## Timing and memory

The current clock configuration is 125 MHz CPU/peripheral and 62.5 MHz SPI
(the SDK divider is checked and the actual rate is reported). A 320x240
RGB565 framebuffer occupies 153,600 bytes of SRAM. The display update is a
full-frame blocking transfer; reported FPS therefore depends on SPI transfer
time plus drawing and loop overhead. The earlier Test_Proj benchmark's 42 FPS
observation is not a measurement of this new multi-game firmware.

The display and framebuffer dimensions are landscape (320x240); the physical
panel glass is commonly described as 240x320 in portrait orientation. Verify
orientation and colors on hardware after flashing. The display still uses
blocking SPI; only audio uses DMA. There is no score persistence or
enclosure-dependent power management.
