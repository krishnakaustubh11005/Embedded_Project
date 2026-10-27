# Architecture

Phase 0 runs one loop on core 0: LED on, wait 500 ms, LED off, wait 500 ms,
print the counter over USB, then increment it. Core 1 is not used yet.

`include/config.h` holds hardware and timing settings; `src/main.c` uses them.
CMake builds the firmware with the Pico SDK for `PICO_BOARD=pico2`.

`Test_Proj/` builds independently as a learning sandbox. It is not linked into
the actual firmware. Rendering, input, audio, and storage modules will be added
in later phases after their concepts are covered.
