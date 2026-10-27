# Build the Pong console firmware for the non-wireless Pico 2.
cmake_minimum_required(VERSION 3.13)

# Load the SDK before project() so it can select the ARM compiler.
if(NOT PICO_SDK_PATH)
    set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
endif()
include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
project(pio_pong C CXX ASM)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

add_executable(pio_pong src/main.c)
target_include_directories(pio_pong PRIVATE include)
target_link_libraries(pio_pong pico_stdlib)

# Enable printf over USB; leave the UART pins free for the future link.
pico_enable_stdio_usb(pio_pong 1)
pico_enable_stdio_uart(pio_pong 0)
pico_add_extra_outputs(pio_pong)
