/* All games share a small callback contract; state is owned by the application. */
#ifndef CONSOLE_GAME_H
#define CONSOLE_GAME_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "platform/input.h"
#include "audio/sound.h"

/* Number of playable games; the launcher's next entry is diagnostics. */
#define GAME_COUNT 4u

typedef struct {
    const char *name, *tagline;
    const char *rules[7];
    void (*reset)(void *state, uint32_t seed);
    void (*input)(void *state, const InputSnapshot *input);
    void (*update)(void *state, const InputSnapshot *input, float seconds);
    void (*draw)(const void *state);
    bool (*finished)(const void *state);
    const char *(*result)(const void *state);
    uint16_t color;                       // Launcher tile and rules banner colour.
    void (*icon)(int cx, int cy);         // Small launcher picture centred at cx,cy.
    int (*winner)(const void *state);     // 0/1 for a player, -1 for none/draw.
    MusicId music;                        // Background loop while playing (MUSIC_NONE: silent).
    bool sound_themes;                    // Rules screen offers C to pick a sound theme.
} GameSpec;
/** Return one of the GAME_COUNT immutable descriptors; index must be below GAME_COUNT. */
const GameSpec *game_get(unsigned int index);
#endif
