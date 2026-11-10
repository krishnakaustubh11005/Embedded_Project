# Live Embedded Explorer — website plan

Date: 2026-10-31. User requests a visualization site for the current modules,
their communications, and clickable explanations. User explicitly requires
everything live. This document plans the work; implementation and deployment
have not started. This is a learning tool alongside the Pico game project.

## Experience

A dark electronics workbench with a large, readable board diagram, colour-coded
connections, restrained activity highlights, and a right-hand inspector. Keep
the view useful for learning and troubleshooting, not decorative animation.
On narrower screens, selecting a module opens its inspector below the diagram.
Keyboard selection and text labels accompany all wire colours.

- Top: USB connection state, firmware identity, last update age, FPS, actual
  SPI clock and framebuffer size, populated from device reports.
- Centre: Pico 2 with individually selectable pins, Funduino joystick shield,
  ST7789 display, common ground and power connections.
- Left: module inventory with status and freshness of received data.
- Right: selected module/pin/event, latest value, timestamp, pin mapping,
  relevant C function and a short explanation of what that activity means.
- Bottom: live, filterable event timeline and plots of timestamped samples.

Interaction examples:

1. Select the joystick: show actual X/Y ADC readings, a position marker and
   history. Centre estimates can refer to the recorded calibration, explicitly
   labelled historical calibration rather than a current reading.
2. Select a button: show the actual reported GPIO level and raw pressed/released
   interpretation. Debounced state appears only when firmware implements it.
3. Select the display: show live draw/transfer timing, current frame number,
   dimensions, actual SPI rate, last reported command and address window.
4. Select an SPI transaction: decode actual reported command bytes and
   parameters. For example, a reported Memory Write command (0x2C) explains
   that following data updates the selected pixel window. Colour decoding uses
   actual captured pixel values, not invented traffic.
5. Select a wire: show endpoints, direction, purpose and latest relevant event.
   Activity highlights are event indicators, not sampled electrical signals.
6. Select the framebuffer: explain row-major indexing, show allocated bytes,
   and inspect a real device snapshot when available, with capture time.

## Current inventory and confidence

| Item | Current project evidence | Site treatment |
|---|---|---|
| Pico 2, RP2350, non-wireless | Confirmed board | USB connection verified by live serial handshake |
| 2.8-inch ST7789, native 240x320 | Confirmed controller and working output | Landscape 320x240, live firmware-reported activity |
| Display CS/SCK/MOSI/DC/RESET | Configured GP17/18/19/20/21 | Load pin map from a matching firmware manifest |
| Joystick X/Y | Tested on GP26/ADC0, GP27/ADC1 | Needs a firmware target that samples both while rendering |
| Shield buttons | Individually tested by moving shield wire on GP2 | Per-button GPIO wiring must be confirmed; do not claim all are connected |
| GP3/4/5 and GP9 | Present in practice configuration | Configured status until actual mapping is confirmed |
| GP6/7 | Disabled in current config | Inactive/unconfirmed, not live inputs |
| Display VCC/backlight | Working module; exact new supply details undocumented | Do not reuse the old SmartElex 5 V instructions |
| Touch and SD | No established wiring/firmware | Excluded from the active module diagram |

A serial-connected Pico cannot automatically identify every passive module
on its GPIO pins. Module topology comes from confirmed wiring and the running
firmware manifest. Live telemetry indicates sampled/reported activity; GPIO
levels alone do not prove which physical module is attached.

## Live data architecture

Pico instrumentation -> USB CDC serial -> browser Web Serial -> bounded event
store -> SVG board/wires, charts, inspector and optional framebuffer view.

Start with a lightweight static HTML/CSS/JavaScript app. Use a supported desktop
browser and a Connect Pico button that invokes the serial chooser. Web Serial
requires a secure context and browser support; show a clear unavailable state
when unsupported. A local serial-to-WebSocket bridge can be a later fallback
if the user's browser lacks Web Serial. The UI must not depend on a cloud
server discovering the USB device.

References: [Chrome Web Serial guide](https://developer.chrome.com/docs/capabilities/serial),
[Web Serial API and support requirements](https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API).

The existing FPS lines can populate performance cards immediately. Current
firmware does not concurrently sample all shield inputs or emit SPI events,
so full functionality requires an additional practice telemetry target.

The new target should send versioned, newline-delimited records with sequence
number, MCU timestamp and event type. Use records for firmware/pin manifest,
input samples, frame statistics and selected SPI command events. The website
must tolerate partial lines, malformed records and unknown fields; buffer
limits prevent unbounded memory growth. Sequence gaps and stale data are
visible. Unknown values display as unavailable, never invented zeros.

Report requested and actual SPI rates separately. Retain the real 6.17 FPS
baseline as a dated comparison; performance at higher clocks remains pending.

## What live means here

- ADC and button values are samples taken by firmware, with actual cadence
  and timestamps. The current blocking renderer limits how often its main
  loop can sample inputs; do not promise a faster input rate than measured.
- Firmware instrumentation reports commands it issues and transfer timings.
  It does not prove the panel acknowledged them or sample every physical edge.
- Actual SCK/MOSI/CS electrical waveforms require a logic analyzer or similar
  capture source. No generated waveform may be presented as a measured trace.
- A display preview must come from actual framebuffer captures. Start with
  requested snapshots or a measured low capture rate. Do not animate a copy
  of the rectangle and label it a live framebuffer stream.
- USB tracing and snapshots add workload. Publish telemetry mode and sample
  rates; measure their FPS effect. Benchmark mode must retain a low-overhead
  option so tracing does not invalidate timing comparisons.
- Hardware buttons on the screen open explanations. Hardware control writes
  are outside the initial scope.

On USB removal, stop activity highlights, mark last values stale and show
waiting for Pico. Reconnect to an already authorized device when supported;
otherwise prompt for an explicit reconnect action. Never switch to simulation.
Only one serial reader should own the port during normal use; replace the
existing miniterm/Python monitor with this website while connected.

## Delivery sequence

1. Build the diagram, inspector, live USB connection lifecycle and parser for
   existing FPS logs. Disconnected UI shows topology but no fake live values.
2. Add a separate readable telemetry firmware target, preserving the FPS
   baseline and performance benchmark. Confirm firmware manifest matches
   actual wiring before showing modules as active.
3. Connect real joystick/button samples and instrumented display events to
   the diagram and charts. Show both raw values and understandable units.
4. Add RGB565 pixel inspection and real framebuffer snapshots; measure the
   effect on frame rate and select a suitable capture cadence.
5. Validate with connected hardware, unplug/replug, missing modules, malformed
   serial records and longer runs. Document supported browser and measured
   sampling/capture rates. Add logic-analyzer import only if requested later.

## Acceptance checks

- A selected wire or module explains its real role and uses the correct pins.
- Moving/pressing a wired input changes its timestamped value in the site.
- FPS and SPI values agree with the device reports and identify their source.
- No SPI waveform or screen preview is fabricated from an animation model.
- No current reading is silently replaced with a historical or calculated one.
- Unplugging makes the site visibly disconnected and stops live indicators.
- Telemetry overhead and browser/device limits are documented after measurement.
- The site remains a learning side tool; Phase 4 and the pending maximum-FPS
  hardware results remain separate outstanding project work.
