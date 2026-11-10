# Pico Arcade: shared portable core plus a small Pico hardware boundary.
cmake_minimum_required(VERSION 3.13)
option(CONSOLE_HOST_ONLY "Build portable rule/UI tests without the Pico SDK" OFF)
option(CONSOLE_SANITIZERS "Use address/undefined sanitizers for host checks" ON)
set(CORE_SOURCES
    src/platform/input.c src/platform/line_parser.c
    src/graphics/framebuffer.c src/graphics/gfx.c
    src/audio/sound.c src/audio/duel_clips.c
    src/diagnostics/metrics.c src/app/app.c src/app/screens.c src/app/game_registry.c
    src/games/physics.c src/games/snake/snake.c src/games/snake/snake_game.c
    src/games/snake/snake_render.c src/games/pong/pong.c src/games/pong/pong_render.c
    src/games/soccer_pool/soccer.c src/games/soccer_pool/soccer_render.c
    src/games/fighter/fighter.c src/games/fighter/fighter_pose.c src/games/fighter/fighter_render.c)
if(CONSOLE_HOST_ONLY)
    project(game_console_host C)
    set(CMAKE_C_STANDARD 11)
    enable_testing()
    add_library(console_core STATIC ${CORE_SOURCES})
    target_include_directories(console_core PUBLIC include)
    target_link_libraries(console_core PUBLIC m)
    target_compile_options(console_core PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    if(CONSOLE_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(console_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(console_core PUBLIC -fsanitize=address,undefined)
    endif()
    add_executable(console_tests tests/unit/console_tests.c)
    target_link_libraries(console_tests PRIVATE console_core)
    target_compile_options(console_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    add_test(NAME console_rules_and_ui COMMAND console_tests)
    set_tests_properties(console_rules_and_ui PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
    return()
endif()

if(NOT PICO_BOARD)
    set(PICO_BOARD pico2 CACHE STRING "Pico 2 is the supported console board")
endif()
if(NOT PICO_SDK_PATH)
    set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
endif()
include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
project(game_console C CXX ASM)
set(CMAKE_C_STANDARD 11)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Optimized firmware with debug symbols" FORCE)
endif()
pico_sdk_init()
add_executable(game_console src/main.c src/platform/board.c src/platform/st7789.c
    src/platform/audio_pwm.c ${CORE_SOURCES})
target_include_directories(game_console PRIVATE include)
target_link_libraries(game_console pico_stdlib hardware_spi hardware_adc hardware_clocks
    hardware_pwm hardware_dma hardware_irq m)
target_compile_definitions(game_console PRIVATE FRAME_TRANSFER_16BIT=1)
target_compile_options(game_console PRIVATE -Wall -Wextra $<$<CONFIG:Debug>:-Og> $<$<CONFIG:Debug>:-g3>)
pico_enable_stdio_usb(game_console 1)
pico_enable_stdio_uart(game_console 0)
pico_set_program_name(game_console "Pico Arcade")
pico_set_program_version(game_console "1.0")
pico_add_extra_outputs(game_console)
# Keep the old build target usable; the artifact is now game_console.uf2.
add_custom_target(pio_pong DEPENDS game_console)
