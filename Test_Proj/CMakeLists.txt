# Build the Phase 0 learning exercise for the non-wireless Pico 2.
cmake_minimum_required(VERSION 3.13)

# Load the SDK before project() so it can select the ARM compiler.
if(NOT PICO_SDK_PATH)
    set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
endif()
include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
project(phase0_blink C CXX ASM)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

add_executable(phase0_blink src/main.c)
target_include_directories(phase0_blink PRIVATE include)
target_link_libraries(phase0_blink pico_stdlib)

# Send printf output over USB instead of the UART pins.
pico_enable_stdio_usb(phase0_blink 1)
pico_enable_stdio_uart(phase0_blink 0)

# Generate the UF2 for BOOTSEL flashing, plus other build outputs.
pico_add_extra_outputs(phase0_blink)
