/* Portable sound engine: games ask for sounds by name, a small synthesizer
 * turns them into PWM sample levels. No GPIO, PWM, DMA or timers here, so the
 * same code runs inside the Pico's DMA interrupt and in PC unit tests.
 *
 * Threading model (single core): game code calls sound_play()/sound_music()
 * from the main loop; these only push a byte into a small queue. The audio
 * interrupt calls sound_render(), which drains the queue and mixes voices.
 */
#ifndef CONSOLE_SOUND_H
#define CONSOLE_SOUND_H
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SND_NONE,
    /* Console UI */
    SND_MENU_MOVE, SND_MENU_SELECT, SND_MENU_BACK, SND_GAME_POP,
    SND_PAUSE, SND_RESUME, SND_FANFARE, SND_GAME_OVER,
    /* Snake */
    SND_SNAKE_TURN, SND_SNAKE_EAT, SND_SNAKE_DIE,
    /* Pong */
    SND_PONG_PADDLE, SND_PONG_WALL, SND_PONG_SCORE,
    /* Soccer Pool */
    SND_SOCCER_SELECT, SND_SOCCER_POWER, SND_SOCCER_SHOT, SND_SOCCER_CLACK,
    SND_SOCCER_GOAL, SND_SOCCER_TURN,
    /* Shade Duel */
    SND_DUEL_WHOOSH, SND_DUEL_SPECIAL, SND_DUEL_JUMP, SND_DUEL_DENIED,
    SND_DUEL_PUNCH_HIT, SND_DUEL_KICK_HIT, SND_DUEL_HEAVY_HIT, SND_DUEL_BLOCK,
    SND_DUEL_ROUND, SND_DUEL_FIGHT, SND_DUEL_KO,
    SND_COUNT
} SoundId;

typedef enum { MUSIC_NONE, MUSIC_NINJA, MUSIC_COUNT } MusicId;

/* Shade Duel impact sounds: synthesized ninja set, or the recorded clips. */
typedef enum { DUEL_THEME_NINJA, DUEL_THEME_CLIPS, DUEL_THEME_COUNT } DuelTheme;

/** Build lookup tables and silence every voice; call once before audio starts. */
void sound_init(void);
/** Queue a one-shot effect; never blocks. Drops the request if the queue is full. */
void sound_play(SoundId id);
/** Queue a background loop (MUSIC_NONE stops it); restarts from the first bar. */
void sound_music(MusicId id);
/** Freeze or continue the current music without losing its position. */
void sound_music_pause(bool paused);
/** Choose which Shade Duel impact set later hits use. */
void sound_set_duel_theme(DuelTheme theme);
/** Return the current Shade Duel impact set. */
DuelTheme sound_duel_theme(void);
/** Return a short constant label for a theme, e.g. "NINJA". */
const char *sound_theme_name(DuelTheme theme);
/** Silence (true) or enable (false) all output; queued requests are discarded while muted. */
void sound_set_muted(bool muted);
/** Return whether output is muted. */
bool sound_muted(void);
/** Return the music most recently requested from the main loop. */
MusicId sound_current_music(void);
/** Return how many times id was requested since sound_init (debug and tests). */
uint32_t sound_play_count(SoundId id);
/** Mix count samples into levels[], each 0..AUDIO_PWM_WRAP with silence at the middle.
 * @note Interrupt-safe: no allocation, no blocking, integer maths only.
 */
void sound_render(uint16_t *levels, unsigned int count);
#endif
