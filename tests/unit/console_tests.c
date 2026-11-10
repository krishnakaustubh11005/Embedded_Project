/* Host checks exercise real portable code, not simulated replacements of game rules. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "app/app.h"
#include "platform/line_parser.h"
#include "graphics/framebuffer.h"
#include "graphics/gfx.h"
#include "audio/sound.h"
#include "audio/clips.h"

/** Compare finite floats within a caller-selected tolerance. */
static bool near(float a,float b,float tolerance) { return isfinite(a)&&isfinite(b)&&fabsf(a-b)<tolerance; }

/** Verify centres are independent, boot holds are quiet, events debounce and recalibration works. */
static void test_input(void) {
    InputProcessor processor;
    input_init(&processor);
    uint16_t raw[2][2]={{2100,2090},{1950,2150}};
    uint8_t keys[2]={KEY_A,0};
    input_process(&processor,raw,keys,0);
    assert(processor.snapshot.player[0].pressed==0);
    for (unsigned int i=1;i<32;i++) input_process(&processor,raw,keys,i*25000);
    assert(processor.snapshot.calibrated && processor.snapshot.centre_valid==15);
    assert(processor.snapshot.player[1].centre[0]==1950);
    assert(processor.snapshot.player[0].x==0 && processor.snapshot.player[1].y==0);
    keys[0]=0; input_process(&processor,raw,keys,800000);
    input_process(&processor,raw,keys,819999); assert(processor.snapshot.player[0].released==0);
    input_process(&processor,raw,keys,820000); assert(processor.snapshot.player[0].released==KEY_A);
    keys[0]=KEY_B; keys[1]=KEY_D;
    input_process(&processor,raw,keys,830000);
    keys[0]=0; input_process(&processor,raw,keys,835000); // bounce
    keys[0]=KEY_B; input_process(&processor,raw,keys,840000);
    input_process(&processor,raw,keys,860000);
    assert(processor.snapshot.player[0].pressed==KEY_B);
    assert(processor.snapshot.player[1].pressed==KEY_D);
    input_process(&processor,raw,keys,885000);
    assert(processor.snapshot.player[0].pressed==0);
    raw[0][0]=4095; raw[0][1]=0; raw[1][0]=1950; raw[1][1]=2150;
    input_process(&processor,raw,keys,910000);
    assert(near(processor.snapshot.player[0].x,1,0.001f));
    assert(near(processor.snapshot.player[0].y,-1,0.001f));
    assert(processor.snapshot.player[1].x==0 && processor.snapshot.player[1].y==0);
    input_recalibrate(&processor); assert(!processor.snapshot.calibrated);
    for (unsigned int i=0;i<32;i++) input_process(&processor,raw,keys,1000000+i*25000);
    assert(processor.snapshot.centre_valid==12); // P1 rail-like axes are flagged.
    assert(processor.snapshot.player[0].centre[0]==2048);
}

/** Check command framing, exact full text, CRLF, editing, overflow and recovery. */
static void test_parser(void) {
    LineParser parser={0};
    const char *secret="DEV 2351\b0";
    for (;*secret;secret++) assert(line_parser_feed(&parser,*secret)==LINE_NONE);
    assert(line_parser_feed(&parser,'\r')==LINE_READY);
    assert(strcmp(parser.text,"DEV 2350")==0);
    assert(line_parser_feed(&parser,'\n')==LINE_NONE);
    for (unsigned int i=0;i<80;i++) assert(line_parser_feed(&parser,'X')==LINE_NONE);
    assert(line_parser_feed(&parser,'\n')==LINE_TOO_LONG);
    const char *help="HELP";
    for (;*help;help++) line_parser_feed(&parser,*help);
    assert(line_parser_feed(&parser,'\n')==LINE_READY && strcmp(parser.text,"HELP")==0);
}

/** Check circle impulses and exact swept face/corner contacts, including large displacements. */
static void test_physics(void) {
    Body a={{0,0},{10,0},2,1},b={{3,0},{0,0},2,1};
    assert(physics_circle_pair(&a,&b,1));
    assert(near(a.velocity.x,0,0.001f) && near(b.velocity.x,10,0.001f));
    assert(physics_length((Vec2){b.position.x-a.position.x,b.position.y-a.position.y})>=3.999f);
    SweepHit hit;
    assert(physics_sweep_rect((Vec2){0,50},(Vec2){1000,0},4,(Rect){100,40,8,36},&hit));
    assert(near(hit.time,0.096f,0.00001f) && hit.normal.x==-1);
    assert(physics_sweep_rect((Vec2){20,20},(Vec2){20,20},4,(Rect){40,40,8,36},&hit));
    assert(near(hit.normal.x,-0.7071f,0.001f) && near(hit.normal.y,-0.7071f,0.001f));
    assert(!physics_sweep_rect((Vec2){44,118},(Vec2){10,0},4,(Rect){32,100,8,36},&hit));
    Vec2 limited=physics_limit((Vec2){1,1},1);
    assert(near(physics_length(limited),1,0.001f));
}

/** Configure a reproducible ball approaching P1's front face for centre/edge tests. */
static void pong_fixture(PongGame *game,float y) {
    pong_reset(game,1); game->serve_seconds=0;
    game->paddle[0]=(Rect){32,100,8,36};
    game->ball.position=(Vec2){60,y}; game->ball.velocity=(Vec2){-200,0};
}

/** Verify both movement axes, speed normalization, half bounds, rebounds, walls and scoring. */
static void test_pong(void) {
    PongGame game; InputSnapshot input={0};
    pong_reset(&game,1); game.serve_seconds=100;
    input.player[0].x=1; input.player[0].y=1;
    pong_update(&game,&input,1.0f/120);
    assert(near(physics_length(game.paddle_velocity[0]),PONG_PADDLE_SPEED,0.01f));
    assert(game.paddle[0].x>32 && game.paddle[0].y<110);
    for (unsigned int i=0;i<500;i++) pong_update(&game,&input,1.0f/120);
    assert(game.paddle[0].x==148 && game.paddle[0].y==40);
    input.player[1].x=-1; input.player[1].y=-1;
    for (unsigned int i=0;i<500;i++) pong_update(&game,&input,1.0f/120);
    assert(game.paddle[1].x==164 && game.paddle[1].y==180);
    memset(&input,0,sizeof(input));
    pong_fixture(&game,118);
    for (unsigned int i=0;i<15;i++) pong_update(&game,&input,1.0f/120);
    assert(game.hits==1 && game.ball.velocity.x>0 && near(game.ball.velocity.y,0,0.01f));
    pong_fixture(&game,102);
    for (unsigned int i=0;i<15;i++) pong_update(&game,&input,1.0f/120);
    assert(game.hits==1 && game.ball.velocity.x>0 && game.ball.velocity.y<0);
    assert(physics_length(game.ball.velocity)<=PONG_MAX_SPEED+0.01f);
    // Aim at the moving paddle's centre at impact, not its initial centre.
    pong_fixture(&game,100.4f); input.player[0].y=1;
    for (unsigned int i=0;i<15;i++) pong_update(&game,&input,1.0f/120);
    assert(game.hits==1 && game.ball.velocity.y<0);
    memset(&input,0,sizeof(input)); pong_fixture(&game,45);
    game.ball.position=(Vec2){160,45}; game.ball.velocity=(Vec2){100,-200};
    for (unsigned int i=0;i<5;i++) pong_update(&game,&input,1.0f/120);
    assert(game.ball.velocity.y>0);
    pong_reset(&game,1);
    for (unsigned int point=0;point<PONG_WIN_SCORE;point++) {
        game.serve_seconds=0;
        game.ball.position=(Vec2){315,80}; game.ball.velocity=(Vec2){320,0};
        pong_update(&game,&input,1.0f/120);
        assert(game.score[0]==point+1);
    }
    assert(game.over && game.winner==0);
    pong_reset(&game,1); game.serve_seconds=0;
    game.ball.position=(Vec2){5,80}; game.ball.velocity=(Vec2){-320,0};
    pong_update(&game,&input,1.0f/120);
    assert(game.score[1]==1 && game.serve_seconds>0);
}

/** Verify selection, inactive-player rejection, power, settling, goals and own goals. */
static void test_soccer(void) {
    SoccerGame game; InputSnapshot input={0};
    soccer_reset(&game,0);
    input.player[1].pressed=KEY_B;
    soccer_input(&game,&input); assert(!game.moving);
    input.player[0].pressed=KEY_A; soccer_input(&game,&input); assert(game.selected==4);
    input.player[0].pressed=KEY_C; soccer_input(&game,&input); assert(game.selected==0);
    input.player[0].pressed=KEY_D; soccer_input(&game,&input); assert(game.power==2);
    input.player[0].pressed=KEY_B; input.player[0].x=1;
    soccer_input(&game,&input); assert(game.moving);
    assert(near(game.bodies[0].velocity.x,180,0.001f));
    assert(!soccer_shoot(&game));
    unsigned int selection=game.selected;
    input.player[0].pressed=KEY_C; soccer_input(&game,&input); assert(game.selected==selection);
    memset(&input,0,sizeof(input));
    for (unsigned int i=0;i<11;i++) game.bodies[i].velocity=(Vec2){0,0};
    for (unsigned int i=0;i<25;i++) soccer_update(&game,&input,1.0f/120);
    assert(!game.moving && game.turn==1);
    soccer_reset(&game,0); game.turn=1;
    game.bodies[10].position=(Vec2){317,128}; game.moving=true;
    soccer_update(&game,&input,1.0f/120);
    assert(game.score[0]==1 && game.turn==1 && !game.moving); // P2 own goal.
    for (unsigned int i=1;i<SOCCER_WIN_SCORE;i++) {
        game.bodies[10].position=(Vec2){317,128}; game.moving=true;
        soccer_update(&game,&input,1.0f/120);
    }
    assert(game.over && game.winner==0);
    soccer_reset(&game,0); game.bodies[10].position=(Vec2){3,128}; game.moving=true;
    soccer_update(&game,&input,1.0f/120);
    assert(game.score[1]==1 && game.turn==0);
    soccer_reset(&game,0); game.bodies[10].position=(Vec2){7,104}; game.moving=true;
    soccer_update(&game,&input,1.0f/120);
    assert(game.score[0]==0 && game.score[1]==0 && game.bodies[10].position.x>=12);
    soccer_reset(&game,0); game.selected=3; game.power=3;
    assert(soccer_shoot(&game));
    for (unsigned int tick=0;tick<4000 && game.moving;tick++) {
        soccer_update(&game,&input,1.0f/120);
        for (unsigned int i=0;i<11;i++) {
            assert(isfinite(game.bodies[i].position.x) && isfinite(game.bodies[i].velocity.y));
            assert(game.bodies[i].position.y>=39.99f && game.bodies[i].position.y<=216.01f);
        }
    }
    assert(!game.moving);
}

/** Inject one already-debounced P1 event without depending on hardware polling. */
static void tap(App *app,uint8_t key) {
    InputSnapshot input={.calibrated=true}; input.player[0].pressed=key;
    app_input(app,&input);
}

/** Exercise real navigation, static rules, pause, rematch, DEV isolation and all renders. */
static void test_app(void) {
    App app; InputSnapshot input={.calibrated=true}; PerfReport report={.spi_hz=62500000};
    app_init(&app,1); app_draw(&app,&input,&report);
    app_input(&app,&input); assert(app.screen==SCREEN_MENU);
    for (unsigned int game=0;game<GAME_COUNT;game++) {
        app.selected=game; app.screen=SCREEN_MENU;
        tap(&app,KEY_B); assert(app.screen==SCREEN_RULES);
        for (unsigned int i=0;i<7;i++) {
            const char *line=game_get(game)->rules[i];
            if (line) assert(strlen(line)*8<=288);
        }
        app_draw(&app,&input,&report);
        app_update(&app,&input,1000000); assert(app.screen==SCREEN_RULES);
        DuelTheme theme=sound_duel_theme();
        tap(&app,KEY_C);   // Only games with sound themes react to C here.
        assert((sound_duel_theme()!=theme)==game_get(game)->sound_themes);
        app_draw(&app,&input,&report);
        sound_set_duel_theme(theme);
        tap(&app,KEY_D); assert(app.screen==SCREEN_MENU);
        uint32_t pops=sound_play_count(SND_GAME_POP);
        tap(&app,KEY_B); tap(&app,KEY_B); assert(app.screen==SCREEN_PLAY);
        assert(sound_play_count(SND_GAME_POP)==pops+1 && sound_current_music()==game_get(game)->music);
        app_update(&app,&input,10000);
        app_draw(&app,&input,&report);
        if (game==0) {
            assert(strcmp(app_command(&app,"DEV 2350"),"DEV ON: slower movement and wall wrap")==0);
            assert(app.game.snake.game.developer);
        } else {
            app_command(&app,"DEV 2350"); assert(!app.snake_assist);
        }
        InputSnapshot pause={0}; pause.player[1].pressed=KEY_MENU;
        app_input(&app,&pause); assert(app.screen==SCREEN_PAUSE);
        GameStorage frozen=app.game;
        app_update(&app,&input,1000000); assert(memcmp(&frozen,&app.game,sizeof(frozen))==0);
        app_draw(&app,&input,&report);
        tap(&app,KEY_D); assert(app.screen==SCREEN_PLAY);
        app_update(&app,&input,1000000); // Resume discards stale paused time.
        assert(memcmp(&frozen,&app.game,sizeof(frozen))==0);
        app_command(&app,"RESTART"); assert(app.screen==SCREEN_PLAY);
        app_command(&app,"MENU"); assert(app.screen==SCREEN_MENU && !app.snake_assist);
        assert(sound_current_music()==MUSIC_NONE);
    }
    assert(game_get(3)->sound_themes && game_get(3)->music==MUSIC_NINJA);
    app.selected=0; tap(&app,KEY_B); tap(&app,KEY_B);
    app_update(&app,&input,0);
    assert(app_update(&app,&input,1000000)>0);
    app.game.snake.game.status=SNAKE_DEAD;
    app_update(&app,&input,10000); assert(app.screen==SCREEN_RESULTS);
    app_draw(&app,&input,&report);
    tap(&app,KEY_B); assert(app.screen==SCREEN_PLAY && app.game.snake.game.score==0);
    app_command(&app,"MENU");
    app.selected=GAME_COUNT; tap(&app,KEY_B); assert(app.screen==SCREEN_DIAGNOSTICS);
    tap(&app,KEY_A); assert(app.recalibrate_requested);
    app_draw(&app,&input,&report);
    tap(&app,KEY_D); assert(app.screen==SCREEN_MENU && app.selected==0);
}

/** Confirm the adapted Snake timer, legal turns and growth use the validated rules. */
static void test_snake(void) {
    SnakeSession session; InputSnapshot input={0};
    snake_spec.reset(&session,1);
    unsigned int x=session.game.body[0].x;
    session.game.food=(SnakeCell){0,0};
    for (unsigned int i=0;i<15;i++) snake_spec.update(&session,&input,1.0f/120);
    assert(session.game.body[0].x==x+1);
    input.player[0].pressed=KEY_D; snake_spec.input(&session,&input);
    assert(!session.game.turn_queued); // Left cannot reverse a right-moving snake.
    input.player[0].pressed=KEY_A; snake_spec.input(&session,&input);
    assert(session.game.turn_queued);
    snake_session_assist(&session,true);
    float previous=session.elapsed;
    snake_spec.update(&session,&input,0.125f);
    assert(session.elapsed>previous && session.game.direction==SNAKE_RIGHT);
    snake_spec.update(&session,&input,0.125f);
    assert(session.game.direction==SNAKE_UP);
}

/** Run n fixed 120 Hz ticks with an unchanged controller snapshot. */
static void duel_ticks(FighterGame *game, const InputSnapshot *input, unsigned int n) {
    for (unsigned int i=0;i<n;i++) fighter_update(game,input,1.0f/120);
}

/** Start a match and skip the ROUND/FIGHT announcement. */
static void duel_fixture(FighterGame *game) {
    InputSnapshot idle={0};
    fighter_reset(game,1);
    duel_ticks(game,&idle,(unsigned int)(1.9f*120));
    assert(game->phase==DUEL_FIGHT);
}

/** Press one button for a single frame, then hold the given stick/buttons. */
static void duel_press(FighterGame *game, unsigned int p, uint8_t key, float x, float y, uint8_t held) {
    InputSnapshot input={0};
    input.player[p].pressed=key; input.player[p].x=x; input.player[p].y=y; input.player[p].held=held;
    fighter_input(game,&input);
}

/** Check skeleton grounding/mirroring, every move, blocking rules, rounds and the match. */
static void test_fighter(void) {
    // Forward kinematics: the lowest point sits on the floor and facing mirrors x.
    Fighter f={0}; Pose pose; Skeleton right,left;
    f.action=ACT_IDLE; fighter_pose(&f,0,&pose);
    fighter_skeleton(&pose,100,208,0,1,1,&right);
    fighter_skeleton(&pose,100,208,0,-1,1,&left);
    assert(near(right.head.x-100,100-left.head.x,0.01f) && near(right.head.y,left.head.y,0.01f));
    float lowest=fmaxf(fmaxf(right.toe[0].y,right.toe[1].y),fmaxf(right.ankle[0].y,right.ankle[1].y));
    assert(lowest<=208 && lowest>=203 && right.head.y<150 && right.head.y>130);
    for (unsigned int a=0;a<ACT_COUNT;a++) for (float t=0;t<1.6f;t+=0.05f) {
        f.action=(FighterAction)a; f.action_time=t; fighter_pose(&f,t,&pose);
        fighter_skeleton(&pose,160,208,0,1,1,&right);
        assert(isfinite(right.head.x) && isfinite(right.toe[0].y) && right.hip.y<208);
    }
    // The Brazilian kick's foot climbs above head height before whipping down.
    f.action=ACT_BRAZIL; f.action_time=0.26f; fighter_pose(&f,0,&pose);
    fighter_skeleton(&pose,100,208,0,1,1,&right);
    assert(right.ankle[0].y<right.head.y);

    FighterGame game; InputSnapshot idle={0};
    duel_fixture(&game);
    assert(game.fighter[0].facing==1 && game.fighter[1].facing==-1);
    // Walking forward closes distance; separation stops overlap.
    InputSnapshot walk={0}; walk.player[0].x=1;
    duel_ticks(&game,&walk,300);
    assert(game.fighter[1].x-game.fighter[0].x>=FIGHTER_MIN_SEPARATION-0.02f);
    // Special without energy is refused and flashes the meter.
    duel_fixture(&game); duel_press(&game,0,KEY_D,0,0,0);
    duel_ticks(&game,&idle,1);
    assert(game.fighter[0].action!=ACT_BRAZIL && game.fighter[0].denied>0);
    // Brazilian kick at kicking range: costs 50 energy, deals 20 and knocks down.
    duel_fixture(&game); game.fighter[0].energy=100; game.fighter[0].x=140; game.fighter[1].x=172;
    uint32_t specials=sound_play_count(SND_DUEL_SPECIAL), heavy=sound_play_count(SND_DUEL_HEAVY_HIT);
    duel_press(&game,0,KEY_D,0,0,0); duel_ticks(&game,&idle,1);
    assert(game.fighter[0].action==ACT_BRAZIL && near(game.fighter[0].energy,50,0.01f));
    duel_ticks(&game,&idle,70);
    assert(sound_play_count(SND_DUEL_SPECIAL)==specials+1 && sound_play_count(SND_DUEL_HEAVY_HIT)==heavy+1);
    assert(near(game.fighter[1].health,FIGHTER_MAX_HEALTH-20,0.01f) && game.fighter[1].action==ACT_KNOCKDOWN);
    assert(game.fighter[0].energy>50);
    // Crouching ducks the (high) Brazilian kick.
    duel_fixture(&game); game.fighter[0].energy=100; game.fighter[0].x=140; game.fighter[1].x=172;
    InputSnapshot duck={0}; duck.player[1].y=-1;
    duel_ticks(&game,&duck,2); assert(game.fighter[1].action==ACT_CROUCH);
    duel_press(&game,0,KEY_D,0,0,0); duel_ticks(&game,&duck,90);
    assert(game.fighter[1].health==FIGHTER_MAX_HEALTH);
    // Standing block turns a kick into 15% chip damage; a sweep beats it.
    duel_fixture(&game); game.fighter[0].x=140; game.fighter[1].x=170;
    InputSnapshot guard={0}; guard.player[1].held=KEY_C;
    duel_ticks(&game,&guard,2); assert(game.fighter[1].action==ACT_BLOCK);
    duel_press(&game,0,KEY_B,0,0,0); duel_ticks(&game,&guard,60);
    assert(near(game.fighter[1].health,FIGHTER_MAX_HEALTH-9*0.15f,0.01f));
    float before=game.fighter[1].health;
    duel_ticks(&game,&guard,30);
    guard.player[0].y=-1; duel_press(&game,0,KEY_B,0,-1,0); duel_ticks(&game,&guard,80);
    assert(near(game.fighter[1].health,before-8,0.01f));
    // Crouch-blocking stops the sweep.
    duel_fixture(&game); game.fighter[0].x=140; game.fighter[1].x=168;
    InputSnapshot low={0}; low.player[0].y=-1; low.player[1].y=-1; low.player[1].held=KEY_C;
    duel_ticks(&game,&low,2); assert(game.fighter[1].action==ACT_CROUCH_BLOCK);
    duel_press(&game,0,KEY_B,0,-1,0); duel_ticks(&game,&low,80);
    assert(near(game.fighter[1].health,FIGHTER_MAX_HEALTH-8*0.15f,0.01f));
    // Jab whiffs a croucher but hits a standing opponent; forward+A is an uppercut.
    duel_fixture(&game); game.fighter[0].x=140; game.fighter[1].x=166;
    duel_ticks(&game,&duck,2); duel_press(&game,0,KEY_A,0,0,0); duel_ticks(&game,&duck,40);
    assert(game.fighter[1].health==FIGHTER_MAX_HEALTH);
    duel_press(&game,0,KEY_A,0,0,0); duel_ticks(&game,&idle,40);
    assert(near(game.fighter[1].health,FIGHTER_MAX_HEALTH-6,0.01f) && game.fighter[1].action==ACT_HIT);
    duel_ticks(&game,&idle,60); game.fighter[0].x=140; game.fighter[1].x=166;
    InputSnapshot forward={0}; forward.player[0].x=1;
    duel_press(&game,0,KEY_A,1,0,0); duel_ticks(&game,&forward,1);
    assert(game.fighter[0].action==ACT_UPPERCUT);
    // Back+C rolls away; up jumps and lands; B in the air is a flying kick.
    duel_fixture(&game);
    InputSnapshot back={0}; back.player[0].x=-1;
    float x=game.fighter[0].x;
    duel_press(&game,0,KEY_C,-1,0,KEY_C); duel_ticks(&game,&back,1);
    assert(game.fighter[0].action==ACT_ROLL);
    duel_ticks(&game,&idle,60); assert(game.fighter[0].x<x-50 && game.fighter[0].action==ACT_IDLE);
    InputSnapshot jump={0}; jump.player[0].y=1;
    duel_ticks(&game,&jump,2); assert(game.fighter[0].action==ACT_JUMP && game.fighter[0].height>0);
    duel_press(&game,0,KEY_B,0,1,0); duel_ticks(&game,&idle,1);
    assert(game.fighter[0].action==ACT_FLY_KICK);
    duel_ticks(&game,&idle,120); assert(game.fighter[0].height==0 && game.fighter[0].action==ACT_IDLE);
    // KO ends the round, the next round restores health, two rounds win the match.
    duel_fixture(&game);
    for (unsigned int round=0;round<FIGHTER_ROUNDS_TO_WIN;round++) {
        assert(game.phase==DUEL_FIGHT);
        game.fighter[0].x=140; game.fighter[1].x=166; game.fighter[1].health=4;
        duel_press(&game,0,KEY_A,0,0,0); duel_ticks(&game,&idle,30);
        assert(game.phase==DUEL_ROUND_END && game.knockout && game.fighter[1].action==ACT_KO);
        assert(game.rounds[0]==round+1);
        duel_ticks(&game,&idle,(unsigned int)(3.05f*120));
        if (round+1<FIGHTER_ROUNDS_TO_WIN) {
            assert(game.round==2 && game.fighter[1].health==FIGHTER_MAX_HEALTH);
            duel_ticks(&game,&idle,(unsigned int)(1.9f*120));
        }
    }
    assert(game.over && fighter_spec.finished(&game) && fighter_spec.winner(&game)==0);
    assert(strcmp(fighter_spec.result(&game),"PLAYER 1 WINS")==0);
    // Timeout: more health wins the round.
    duel_fixture(&game); game.fighter[0].health=40; game.time_left=0.005f;
    duel_ticks(&game,&idle,2);
    assert(game.phase==DUEL_ROUND_END && !game.knockout && game.round_winner==1);
    fighter_draw(&game); fighter_icon(40,40);
}

/** Check reporting arithmetic, reset, SPI field preservation and persistent sample history. */
static void test_metrics_and_graphics(void) {
    Metrics metrics; PerfReport report={.spi_hz=62500000}; metrics_init(&metrics,0);
    uint64_t stages[]={100,200,300,124400};
    for (unsigned int i=0;i<8;i++) metrics_record(&metrics,i*125000,i*125000+100,(i+1)*125000,stages,0);
    assert(metrics_report(&metrics,1000000,&report));
    assert(near(report.fps,8,0.001f) && report.average_us[3]==124400);
    assert(report.max_gap_us==125000 && report.over_budget==8 && report.spi_hz==62500000);
    assert(metrics.last_sample==875100);
    gfx_clear(BLACK); gfx_rect(-2,-2,4,4,INK);
    assert(fb_pixels()[0]==INK && fb_pixels()[2]==BLACK);
    gfx_line(-5,10,5,10,ACCENT); assert(fb_pixels()[10*320]==ACCENT);
    gfx_circle(319,239,4,RED,true); assert(fb_pixels()[319+239*320]==RED);
    gfx_clear(BLACK); gfx_capsule(10,10,30,10,3,INK);
    assert(fb_pixels()[10*320+20]==INK && fb_pixels()[13*320+20]==INK && fb_pixels()[14*320+20]==BLACK);
    gfx_round_rect(0,0,20,20,6,ACCENT);
    assert(fb_pixels()[0]==BLACK && fb_pixels()[10*320]==ACCENT && fb_pixels()[10]==ACCENT);
    gfx_dim(0,0,1,1); assert(fb_pixels()[10*320]==ACCENT);
    assert(gfx_mix(BLACK,INK,0)==BLACK && gfx_mix(BLACK,INK,256)==INK);
    assert(gfx_text_width("AB",2)==14);
}

/** Render n samples and report whether any differ from silence; all must be in range. */
static bool audible(unsigned int n) {
    static uint16_t levels[AUDIO_SAMPLE_RATE];
    assert(n<=AUDIO_SAMPLE_RATE);
    sound_render(levels,n);
    bool any=false;
    for (unsigned int i=0;i<n;i++) {
        assert(levels[i]<=AUDIO_PWM_WRAP);
        if (levels[i]!=(AUDIO_PWM_WRAP+1)/2) any=true;
    }
    return any;
}

/** Exercise the real synthesizer: silence, every effect, music pause, clips and mute. */
static void test_sound(void) {
    sound_init();
    assert(!audible(1000));
    for (unsigned int id=SND_NONE+1;id<SND_COUNT;id++) {
        sound_play((SoundId)id);
        assert(audible(AUDIO_SAMPLE_RATE/4));           // Starts promptly.
        for (unsigned int i=0;i<8;i++) (void)audible(AUDIO_SAMPLE_RATE/2);
        assert(!audible(256));                          // And always ends.
        assert(sound_play_count((SoundId)id)==1);
    }
    for (unsigned int i=0;i<40;i++) sound_play(SND_SOCCER_CLACK);  // Queue overflow is safe.
    (void)audible(AUDIO_SAMPLE_RATE);
    sound_music(MUSIC_NINJA);
    assert(audible(AUDIO_SAMPLE_RATE) && sound_current_music()==MUSIC_NINJA);
    for (unsigned int i=0;i<20;i++) assert(audible(AUDIO_SAMPLE_RATE));  // Loops past its end.
    sound_music_pause(true); assert(!audible(512));
    sound_music_pause(false); assert(audible(AUDIO_SAMPLE_RATE/2));
    sound_music(MUSIC_NONE); assert(!audible(512));
    // CLIPS theme: a punch plays the recorded samples exactly.
    sound_set_duel_theme(DUEL_THEME_CLIPS);
    assert(strcmp(sound_theme_name(sound_duel_theme()),"CLIPS")==0);
    static uint16_t levels[256];
    sound_play(SND_DUEL_PUNCH_HIT); sound_render(levels,256);
    for (unsigned int i=0;i<256;i++) {
        int expected=(int)(AUDIO_PWM_WRAP+1)/2+(clip_punch1.data[i]*255/128)*(int)AUDIO_VOLUME/256;
        assert(levels[i]==expected);
    }
    for (unsigned int i=0;i<2;i++) (void)audible(AUDIO_SAMPLE_RATE/2);
    sound_set_duel_theme(DUEL_THEME_NINJA);
    sound_set_muted(true); sound_play(SND_DUEL_KO); assert(!audible(2000));
    sound_set_muted(false);
    assert(clip_kick.length>1000 && clip_punch1.length>1000 && clip_punch2.length>1000);
}

/** Run host assertions with sanitizers; hardware timing and wiring require device checks. */
int main(void) {
    test_input(); test_parser(); test_physics(); test_pong(); test_soccer();
    test_sound();
    test_snake(); test_fighter(); test_app(); test_metrics_and_graphics();
    puts("PASS: sound, input, parser, collisions, Pong, Soccer Pool, Snake, Shade Duel, navigation, metrics and RAM rendering");
    return 0;
}
