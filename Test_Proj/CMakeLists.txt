# Build separate learning exercises for the non-wireless Pico 2.
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

# Build the GP2 button exercise independently of the blink program.
add_executable(phase1_gp2 src/phase1_gp2.c)
target_include_directories(phase1_gp2 PRIVATE include)
target_link_libraries(phase1_gp2 pico_stdlib)
pico_enable_stdio_usb(phase1_gp2 1)
pico_enable_stdio_uart(phase1_gp2 0)
pico_add_extra_outputs(phase1_gp2)

# Extend the button test without replacing the earlier one-pin exercise.
add_executable(phase1_buttons src/phase1_buttons.c)
target_include_directories(phase1_buttons PRIVATE include)
target_link_libraries(phase1_buttons pico_stdlib)
pico_enable_stdio_usb(phase1_buttons 1)
pico_enable_stdio_uart(phase1_buttons 0)
pico_add_extra_outputs(phase1_buttons)

# The ADC exercise needs the SDK's hardware_adc module as well as stdlib.
add_executable(phase1_joystick src/phase1_joystick.c)
target_include_directories(phase1_joystick PRIVATE include)
target_link_libraries(phase1_joystick pico_stdlib hardware_adc)
pico_enable_stdio_usb(phase1_joystick 1)
pico_enable_stdio_uart(phase1_joystick 0)
pico_add_extra_outputs(phase1_joystick)

# Slow GPIO output checks for the five display signal jumper wires.
add_executable(phase1_display_pins src/phase1_display_pins.c)
target_include_directories(phase1_display_pins PRIVATE include)
target_link_libraries(phase1_display_pins pico_stdlib)
pico_enable_stdio_usb(phase1_display_pins 1)
pico_enable_stdio_uart(phase1_display_pins 0)
pico_add_extra_outputs(phase1_display_pins)

# Direct visual test, independent of joystick and game logic.
add_executable(phase2_display src/phase2_display.c src/st7789.c)
target_include_directories(phase2_display PRIVATE include)
target_link_libraries(phase2_display pico_stdlib hardware_spi)
pico_enable_stdio_usb(phase2_display 1)
pico_enable_stdio_uart(phase2_display 0)
target_compile_options(phase2_display PRIVATE -Wall -Wextra)
pico_add_extra_outputs(phase2_display)

# Isolate the remaining landscape columns without replacing the full demo.
add_executable(phase2_strip_test src/phase2_strip_test.c src/st7789.c)
target_include_directories(phase2_strip_test PRIVATE include)
target_link_libraries(phase2_strip_test pico_stdlib hardware_spi)
pico_enable_stdio_usb(phase2_strip_test 1)
pico_enable_stdio_uart(phase2_strip_test 0)
target_compile_options(phase2_strip_test PRIVATE -Wall -Wextra)
pico_add_extra_outputs(phase2_strip_test)

# Slow GP19 high/low output check for a DC multimeter; independent of SPI.
add_executable(phase2_mosi_pin_test src/phase2_mosi_pin_test.c)
target_include_directories(phase2_mosi_pin_test PRIVATE include)
target_link_libraries(phase2_mosi_pin_test pico_stdlib)
pico_enable_stdio_usb(phase2_mosi_pin_test 1)
pico_enable_stdio_uart(phase2_mosi_pin_test 0)
pico_add_extra_outputs(phase2_mosi_pin_test)

# One framebuffer, blocking SPI, one core. Earlier targets retain their slow clock.
add_executable(phase3_framebuffer src/phase3_framebuffer.c src/framebuffer.c src/st7789.c)
target_include_directories(phase3_framebuffer PRIVATE include)
target_link_libraries(phase3_framebuffer pico_stdlib hardware_spi)
target_compile_definitions(phase3_framebuffer PRIVATE DISPLAY_SPI_HZ=10000000)
target_compile_options(phase3_framebuffer PRIVATE -Wall -Wextra)
pico_enable_stdio_usb(phase3_framebuffer 1)
pico_enable_stdio_uart(phase3_framebuffer 0)
pico_add_extra_outputs(phase3_framebuffer)

# Performance sweep using the same image and SRAM buffer as the baseline.
add_executable(phase3_fps_benchmark src/phase3_framebuffer.c src/framebuffer.c src/st7789.c)
target_include_directories(phase3_fps_benchmark PRIVATE include)
target_link_libraries(phase3_fps_benchmark pico_stdlib hardware_spi hardware_clocks)
target_compile_definitions(phase3_fps_benchmark PRIVATE
    DISPLAY_SPI_HZ=10000000 FRAME_TRANSFER_16BIT=1 FPS_BENCHMARK=1)
target_compile_options(phase3_fps_benchmark PRIVATE -Wall -Wextra)
pico_enable_stdio_usb(phase3_fps_benchmark 1)
pico_enable_stdio_uart(phase3_fps_benchmark 0)
pico_add_extra_outputs(phase3_fps_benchmark)

# Small game to measure four-button controls alongside a full display transfer.
add_executable(phase4_snake src/phase4_snake.c src/snake.c src/snake_input.c
    src/snake_ui.c src/framebuffer.c src/st7789.c)
target_include_directories(phase4_snake PRIVATE include)
target_link_libraries(phase4_snake pico_stdlib hardware_spi hardware_clocks)
target_compile_definitions(phase4_snake PRIVATE
    DISPLAY_SPI_HZ=62500000 FRAME_TRANSFER_16BIT=1)
target_compile_options(phase4_snake PRIVATE -Wall -Wextra)
pico_enable_stdio_usb(phase4_snake 1)
pico_enable_stdio_uart(phase4_snake 0)
pico_add_extra_outputs(phase4_snake)
