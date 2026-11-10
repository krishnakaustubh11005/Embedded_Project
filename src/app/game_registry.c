/* Immutable catalog: adding a game does not change board or menu input code. */
#include <assert.h>
#include "app/game.h"
#include "games/snake/snake_game.h"
#include "games/pong/pong.h"
#include "games/soccer_pool/soccer.h"
#include "games/fighter/fighter.h"

/** Return a persistent descriptor: 0 Snake, 1 Pong, 2 Soccer Pool, 3 Shade Duel. */
const GameSpec *game_get(unsigned int index) {
    static const GameSpec *const catalog[GAME_COUNT]={&snake_spec,&pong_spec,&soccer_spec,&fighter_spec};
    assert(index<GAME_COUNT);
    return catalog[index<GAME_COUNT?index:0];
}
