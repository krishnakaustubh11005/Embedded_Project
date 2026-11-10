# Current wiring

Board: non-wireless Raspberry Pi Pico 2 (RP2350). GPIO numbers below are not
the Pico's physical header-pin numbers. Power off USB before changing wires.
All Pico GPIO and ADC signals must stay within the Pico's 3.3 V limits; share
ground between the Pico and each module.

## ST7789 display

The student reports that the 2.8-inch 240x320 V1.1 display's SPI connections
work and identifies its controller as ST7789. The firmware uses landscape
320x240 and SPI0.

| Display signal | Pico GPIO | Pico physical pin |
|---|---:|---:|
| CS | GP17 | 22 |
| SCK | GP18 | 24 |
| MOSI | GP19 | 25 |
| DC | GP20 | 26 |
| RESET | GP21 | 27 |
| VCC | 3V3 OUT | 36 |
| GND | GND | 23 (or another GND pin) |
| Backlight | follow module markings | — |

Keep display power and logic at a level supported by the exact module. Do not
assume a 5 V-tolerant SPI input from a 5 V supply label. The separate XPT2046
touch controller is not used by this firmware.

## Player 1 joystick shield

The Funduino/Nokla 5110 Joystick Shield V1.A is powered from Pico 3V3 OUT and
GND. Keep its selector in 3.3 V mode; do not connect its 5 V rail to the Pico.

| Control | Pico GPIO | Physical pin | Function |
|---|---:|---:|---|
| Joystick X | Through CD4066 to GP28 / ADC2 | 34 | P1 horizontal position |
| Joystick Y | GP27 / ADC1 | 32 | P1 vertical position |
| A | GP2 | 4 | Button A |
| B | GP3 | 5 | Button B |
| C | GP4 | 6 | Button C |
| D | GP5 | 7 | Button D |
| joystick click | GP12 | 16 | Start/pause |
| VCC | 3V3 OUT | 36 | Shield supply |
| GND | GND | 3 (or another GND pin) | Common ground |

## Shared joystick X axes and CD4066BE analog switch

The two joystick X outputs share ADC2 through two CD4066BE switch sections.
Firmware selects P1 X, reads it, then selects P2 X and reads it. P1 and P2 Y
are connected directly to separate ADC inputs. Never enable both X paths
together. Power the CD4066BE from 3.3 V: pin 14 to Pico 3V3 and pin 7 to Pico
GND.

| Signal / switch | CD4066BE connection | Pico connection |
|---|---|---|
| P1 X | Switch A input pin 1; output pin 2 | GP28 / ADC2, physical pin 34 |
| P1 X enable | Switch A control pin 13 | GP14, physical pin 19 |
| P2 X | Switch B input pin 4; output pin 3 | GP28 / ADC2, physical pin 34 (shared with pin 2) |
| P2 X enable | Switch B control pin 5 | GP15, physical pin 20 |
| P2 Y | Direct | GP26 / ADC0, physical pin 31 |
| P1 Y | Direct | GP27 / ADC1, physical pin 32 |
| P2 A | Shield D2 | GP6, physical pin 9 |
| P2 B | Shield D3 | GP7, physical pin 10 |
| P2 C | Shield D4 | GP8, physical pin 11 |
| P2 D | Shield D5 | GP9, physical pin 12 |
| P2 click | Shield D8 | GP13, physical pin 17 |
| Shield VCC/GND | 3V3/GND | Pico 3V3/GND |

The last two button rows beyond A/B are planned mappings; confirm those shield
wires are installed before relying on C/D. The uploaded pinout screenshots
clearly showed P2 A/B connections. Tie unused CD4066BE control inputs (pins 6
and 12) to ground so they cannot float; leave unused analog switch terminals
unconnected unless the chip datasheet and circuit call for otherwise. Keep
analog wiring short and grounds common.

## Input behavior

Buttons use internal pull-ups and are active-low. Firmware applies a 20 ms
software debounce. Joystick centers are calibrated from startup samples and
each ADC reading is averaged. Each X read disables both switches, waits 5 us,
enables only the requested path, selects ADC2, and waits 500 us. Four conversions
are discarded before averaging four fresh conversions; both switches are then
disabled. Direct Y channels wait 10 us after ADC selection and use the same
discard/average sequence. These tuning values are in `include/config.h`.
This reduces residual-charge carryover; averaging alone cannot remove it.
`CAL` over USB requests recalibration with both sticks released.

To check X isolation, keep P2 centered and sweep P1 X end-to-end, then reverse
the test. The stationary player's raw X reading should not follow the moving
stick. If it still does, power off and check that the two X wires reach separate
switch inputs (pins 1 and 4), only the switch outputs (pins 2 and 3) join at
GP28, and controls 13 and 5 are wired separately to GP14 and GP15. Verify the
CD4066 supply is 3.3 V and grounds are common. Firmware delays cannot fix a
shorted input or a switch that remains electrically enabled.

## Speaker (SmartElex digital speaker module)

The module is assumed to be the common 3-pin 40x40 mm board with an on-board
8002-type amplifier: GND, VCC and a signal input (often labelled S, SIG or IN).
Read the silkscreen on your board before wiring; if the labels differ, stop
and check them. The firmware outputs PWM audio on **GP22** (physical pin 29).

| Module pin | Connect to | Notes |
|---|---|---|
| GND | Pico GND (e.g. physical pin 28 or 38) | Common ground with the Pico. |
| VCC | Pico VBUS (physical pin 40, 5 V from USB) | 8002 amps run 2-5.5 V; 5 V is louder. Use 3V3 (pin 36) if the module is marked 3.3 V only. |
| S / SIG / IN | GP22 through the RC filter below | 3.3 V PWM signal; never connect VBUS here. |

Recommended RC low-pass filter between GP22 and the signal pin (removes the
122 kHz PWM carrier so the amplifier is not overdriven by it):

```
GP22 ──[ 1 kΩ ]──┬──► module S/IN
                 │
               [ 10 nF ]        corner ≈ 1/(2π·1k·10n) ≈ 16 kHz
                 │
GND  ────────────┴──► module GND
```

Without the filter it usually still works (the speaker cannot move at
122 kHz), so it is acceptable for a first test. If the module has a volume
knob, start at its middle position. Silence is a steady 50 % duty cycle, so a
multimeter on GP22 in DC volts should read about 1.65 V when nothing plays.

## Validation still needed

Software builds and host tests do not prove the physical wiring. Before game
play, flash the UF2 and check the USB `STATUS` output, button response, joystick
calibration, display orientation/colors, and both X-axis mux paths. If an axis
read incorrectly, power off and verify CD4066 pin numbering against the notch
and the datasheet before changing firmware.
