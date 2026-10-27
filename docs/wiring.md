# Wiring

Confirmed board: Raspberry Pi Pico 2, non-wireless.

Phase 0 uses the onboard LED and a USB data cable; no external wiring is needed.
The SDK board definition maps the LED to GP25. Firmware accesses this through
`LED_PIN` in `include/config.h`.

The external pin map in `Agents.md` is planned wiring. Its connections have not
been verified. Confirm the exact display module and resolution before display code.
