# Measurements

Record observed values with the date, firmware version, hardware setup, and test conditions. A successful build is not proof that the hardware works.

## Phase 0: practice hardware verification

- Board confirmed by student on 2026-10-27: Raspberry Pi Pico 2, non-wireless.
- Practice firmware: `Test_Proj/`.
- 2026-10-27: student reports the onboard LED is blinking with the practice firmware. Blink timing has not been measured.
- 2026-10-27: host-side Python serial reader at `/dev/ttyACM0`, 115200 requested baud, captured `Counter: 337`, `Counter: 338`, and `Counter: 339`. USB output verified; exact output interval has not been measured.
- 2026-10-27: student subsequently confirmed seeing both the LED and serial output. Practice timing is now configured to 100 ms per half-blink; the exact flashed revision was not recorded. The actual project scaffold uses 500 ms per half-blink. Its separate UF2 has not yet been tested on hardware.
