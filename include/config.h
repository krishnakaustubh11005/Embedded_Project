/* Production hardware and tuning. GPIO numbers are not physical header pins. */
#ifndef CONFIG_H
#define CONFIG_H

#define CONSOLE_CPU_KHZ 125000u
#define DISPLAY_SPI spi0
#define DISPLAY_SPI_HZ 62500000u
#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240
#define DISPLAY_CS_PIN 17
#define DISPLAY_SCK_PIN 18
#define DISPLAY_MOSI_PIN 19
#define DISPLAY_DC_PIN 20
#define DISPLAY_RST_PIN 21
#define DISPLAY_MADCTL 0xA0
#define DISPLAY_INVERT_COLORS 0
#define P1_BUTTON_PINS {2, 3, 4, 5, 12}
#define P2_BUTTON_PINS {6, 7, 8, 9, 13}
#define P2_Y_PIN 26
#define P1_Y_PIN 27
#define SHARED_X_ADC_PIN 28
#define P2_Y_ADC 0
#define P1_Y_ADC 1
#define SHARED_X_ADC 2
#define P1_X_ENABLE_PIN 15
#define P2_X_ENABLE_PIN 14
#define MUX_BREAK_US 5u
/* Allow the shared node and ADC input to settle after selecting each X path. */
#define MUX_SETTLE_US 500u
#define ADC_SETTLE_US 10u
#define ADC_DISCARD_SAMPLES 4u
#define ADC_AVERAGE_SAMPLES 4u
#define INPUT_DEBOUNCE_US 20000u
#define INPUT_DEAD_ZONE 150
#define INPUT_CALIBRATION_SAMPLES 32u
#define PHYSICS_HZ 120u
#define MAX_CATCHUP_STEPS 8u
#define FRAME_BUDGET_US (1000000u / 43u)
#define SERIAL_CHARS_PER_FRAME 32u
#define PONG_PADDLE_SPEED 220.0f
#define PONG_INITIAL_SPEED 200.0f
#define PONG_MAX_SPEED 320.0f
#define PONG_WIN_SCORE 7u
#define SOCCER_WIN_SCORE 3u
#define SOCCER_MAX_SHOT 240.0f
#define SOCCER_MAX_SPEED 320.0f
#define SOCCER_FRICTION 80.0f
#define SOCCER_STOP_SPEED 3.0f
#define SOCCER_REST_SECONDS 0.2f
#define SNAKE_CELL_SIZE 8
#define SNAKE_HEADER_HEIGHT 40
/* Audio: GP22 PWM -> RC low-pass -> SmartElex speaker module "S"/IN pin.
 * 125 MHz / (1023+1) = 122 kHz carrier, far above hearing; the DMA timer
 * changes the duty cycle AUDIO_SAMPLE_RATE times per second. */
#define AUDIO_PIN 22
#define AUDIO_SAMPLE_RATE 22050u
#define AUDIO_PWM_WRAP 1023u
#define AUDIO_BLOCK_SAMPLES 256u   // One DMA half-buffer = 11.6 ms of sound.
#define AUDIO_VOLUME 240u          // Master gain 0..256; module knob also sets level.
#define AUDIO_SFX_VOICES 5u
#define AUDIO_MUSIC_BPM 132u

/* Shade Duel: pixels, seconds, pixels/second and hit points. */
#define FIGHTER_GROUND_Y 208.0f
#define FIGHTER_ARENA_LEFT 18.0f
#define FIGHTER_ARENA_RIGHT 302.0f
#define FIGHTER_MIN_SEPARATION 24.0f
#define FIGHTER_MAX_HEALTH 100.0f
#define FIGHTER_ROUNDS_TO_WIN 2u
#define FIGHTER_ROUND_SECONDS 60.0f
#define FIGHTER_WALK_SPEED 74.0f
#define FIGHTER_BACK_SPEED 58.0f
#define FIGHTER_JUMP_SPEED 340.0f
#define FIGHTER_GRAVITY 1150.0f
#define FIGHTER_ROLL_SPEED 175.0f
#define FIGHTER_ENERGY_MAX 100.0f
#define FIGHTER_SPECIAL_COST 50.0f
/* Joystick deflection (0..1) treated as a deliberate direction. */
#define FIGHTER_STICK_THRESHOLD 0.4f
/* A button press is remembered this long if the fighter is still busy. */
#define FIGHTER_INPUT_BUFFER 0.15f

#endif
