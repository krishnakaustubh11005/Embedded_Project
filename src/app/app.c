/* Console state machine: navigation remains independent of game and hardware code. */
#include <math.h>
#include <string.h>
#include "config.h"
#include "app/app.h"

/** Initialize state and seed in RAM; initial input calibration gates the launcher. */
void app_init(App *app, uint32_t seed) {
    memset(app,0,sizeof(*app)); app->seed=seed; app->screen=SCREEN_BOOT;
}

/** Start/restart the selected game, keeping explicit Snake assistance only on rematches. */
static void start_game(App *app) {
    app->seed=app->seed*1664525u+1013904223u;
    const GameSpec *spec=game_get(app->selected);
    spec->reset(&app->game,app->seed);
    if (app->selected==0) snake_session_assist(&app->game.snake,app->snake_assist);
    app->screen=SCREEN_PLAY; app->reset_clock=true; app->accumulator=0;
    sound_play(SND_GAME_POP);
    sound_music(spec->music);
}

/** Return to the launcher and clear cross-match assistance and accumulated time. */
static void return_to_menu(App *app) {
    if (sound_current_music()!=MUSIC_NONE) sound_music(MUSIC_NONE);
    app->screen=SCREEN_MENU; app->snake_assist=false; app->accumulator=0; app->reset_clock=true;
}

/** Open the pause card, freezing music where it is. */
static void pause_game(App *app) {
    app->screen=SCREEN_PAUSE; app->pause_choice=0; app->accumulator=0;
    sound_play(SND_PAUSE); sound_music_pause(true);
}

/** Leave the pause card and continue the frozen match and music. */
static void resume_game(App *app) {
    app->screen=SCREEN_PLAY; app->reset_clock=true;
    sound_play(SND_RESUME); sound_music_pause(false);
}

/** Route frame-level events; game callbacks receive events exactly once per frame.
 * @note P1 navigates menus. Either K pauses play before any gameplay action runs.
 */
void app_input(App *app, const InputSnapshot *input) {
    uint8_t keys=input->player[0].pressed;
    bool pause=((input->player[0].pressed|input->player[1].pressed)&KEY_MENU)!=0;
    switch (app->screen) {
        case SCREEN_BOOT:
            if (input->calibrated) return_to_menu(app);
            break;
        case SCREEN_MENU:
            if (keys&(KEY_A|KEY_C)) sound_play(SND_MENU_MOVE);
            if (keys&KEY_A) app->selected=(app->selected+GAME_COUNT)%(GAME_COUNT+1);
            if (keys&KEY_C) app->selected=(app->selected+1)%(GAME_COUNT+1);
            if (keys&KEY_B) {
                sound_play(SND_MENU_SELECT);
                app->screen=app->selected==GAME_COUNT?SCREEN_DIAGNOSTICS:SCREEN_RULES;
            }
            break;
        case SCREEN_RULES:
            if (keys&KEY_D) { sound_play(SND_MENU_BACK); return_to_menu(app); }
            else if (keys&KEY_B) start_game(app);
            else if ((keys&KEY_C) && game_get(app->selected)->sound_themes) {
                /* Cycle the theme and play a heavy hit so the player hears it at once. */
                sound_set_duel_theme((DuelTheme)((sound_duel_theme()+1u)%DUEL_THEME_COUNT));
                sound_play(SND_DUEL_HEAVY_HIT);
            }
            break;
        case SCREEN_PLAY:
            if (pause) pause_game(app);
            else game_get(app->selected)->input(&app->game,input);
            break;
        case SCREEN_PAUSE:
            if (pause || (keys&KEY_D)) resume_game(app);
            else {
                if (keys&(KEY_A|KEY_C)) sound_play(SND_MENU_MOVE);
                if (keys&KEY_A) app->pause_choice=(app->pause_choice+2)%3;
                if (keys&KEY_C) app->pause_choice=(app->pause_choice+1)%3;
                if (keys&KEY_B) {
                    if (app->pause_choice==0) resume_game(app);
                    else if (app->pause_choice==1) start_game(app);
                    else { sound_play(SND_MENU_BACK); return_to_menu(app); }
                }
            }
            break;
        case SCREEN_RESULTS:
            if (keys&KEY_D) { sound_play(SND_MENU_BACK); return_to_menu(app); }
            else if (keys&KEY_B) start_game(app);
            break;
        case SCREEN_DIAGNOSTICS:
            if (keys&KEY_D) {
                sound_play(SND_MENU_BACK);
                if (app->selected==GAME_COUNT) app->selected=0;
                return_to_menu(app);
            }
            else if (keys&KEY_A) app->recalibrate_requested=true;
            break;
    }
}

/** Update physics independently of rendering, bounded to eight 120 Hz steps.
 * @param elapsed_us Real time since the prior loop; large stalls are capped and
 *       skipped ticks reported rather than causing an unbounded catch-up spiral.
 */
unsigned int app_update(App *app, const InputSnapshot *input, uint64_t elapsed_us) {
    if (app->screen!=SCREEN_PLAY || app->reset_clock) {
        app->accumulator=0; app->reset_clock=false; return 0;
    }
    const float step=1.0f/PHYSICS_HZ;
    unsigned int dropped=0;
    if (elapsed_us>250000) { dropped=(unsigned int)((elapsed_us-250000)*PHYSICS_HZ/1000000); elapsed_us=250000; }
    app->accumulator+=(float)elapsed_us/1000000;
    unsigned int ticks=0;
    const GameSpec *spec=game_get(app->selected);
    while (app->accumulator>=step && ticks<MAX_CATCHUP_STEPS) {
        spec->update(&app->game,input,step);
        app->accumulator-=step; ticks++;
        if (spec->finished(&app->game)) {
            app->screen=SCREEN_RESULTS; app->accumulator=0;
            sound_music(MUSIC_NONE);
            sound_play(spec->winner(&app->game)>=0?SND_FANFARE:SND_GAME_OVER);
            break;
        }
    }
    if (app->accumulator>=step) {
        dropped+=(unsigned int)(app->accumulator/step);
        app->accumulator=fmodf(app->accumulator,step);
    }
    return dropped;
}

/** Apply complete USB commands without reading the port or blocking the loop.
 * @return A persistent response string. Gameplay commands require an active game;
 *         DEV is Snake-only and never silently affects multiplayer matches.
 */
const char *app_command(App *app, const char *command) {
    if (strcmp(command,"HELP")==0) return "HELP MENU DIAG CAL PAUSE RESUME RESTART DEV 2350 DEV OFF MUTE UNMUTE THEME";
    if (strcmp(command,"MUTE")==0) { sound_set_muted(true); return "SOUND OFF"; }
    if (strcmp(command,"UNMUTE")==0) { sound_set_muted(false); return "SOUND ON"; }
    if (strcmp(command,"THEME")==0) {
        sound_set_duel_theme((DuelTheme)((sound_duel_theme()+1u)%DUEL_THEME_COUNT));
        sound_play(SND_DUEL_HEAVY_HIT);
        return sound_duel_theme()==DUEL_THEME_CLIPS?"SHADE DUEL SOUND: CLIPS":"SHADE DUEL SOUND: NINJA";
    }
    if (strcmp(command,"MENU")==0) { app->selected=0; return_to_menu(app); return "MENU"; }
    if (strcmp(command,"DIAG")==0 || strcmp(command,"CAL")==0) {
        app->screen=SCREEN_DIAGNOSTICS; app->accumulator=0;
        if (strcmp(command,"CAL")==0) app->recalibrate_requested=true;
        return "DIAGNOSTICS: A calibrates, D returns";
    }
    bool active=app->selected<GAME_COUNT && (app->screen==SCREEN_PLAY||app->screen==SCREEN_PAUSE||app->screen==SCREEN_RESULTS);
    if (!active) return "Start a game first; HELP lists commands";
    if (strcmp(command,"RESTART")==0) { start_game(app); return "RESTARTED"; }
    if (strcmp(command,"PAUSE")==0 && app->screen==SCREEN_PLAY) { pause_game(app); return "PAUSED"; }
    if (strcmp(command,"RESUME")==0 && app->screen==SCREEN_PAUSE) { resume_game(app); return "RESUMED"; }
    if ((strcmp(command,"DEV 2350")==0||strcmp(command,"DEV OFF")==0) &&
        app->selected==0 && app->screen!=SCREEN_RESULTS) {
        app->snake_assist=strcmp(command,"DEV 2350")==0;
        snake_session_assist(&app->game.snake,app->snake_assist);
        return app->snake_assist?"DEV ON: slower movement and wall wrap":"DEV OFF";
    }
    return "Command unavailable here; HELP lists commands";
}
