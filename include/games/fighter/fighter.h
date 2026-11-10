/* Shade Duel: two-player silhouette fighting game with procedural skeleton animation.
 * Units: pixels, seconds, degrees. Screen y grows downward; "forward" means
 * toward the fighter's facing direction. No hardware access.
 */
#ifndef CONSOLE_FIGHTER_H
#define CONSOLE_FIGHTER_H
#include "app/game.h"
#include "games/physics.h"

typedef enum {
    ACT_IDLE, ACT_WALK, ACT_CROUCH, ACT_BLOCK, ACT_CROUCH_BLOCK, ACT_JUMP,
    ACT_PUNCH, ACT_UPPERCUT, ACT_KICK, ACT_SWEEP, ACT_FLY_KICK, ACT_BRAZIL,
    ACT_ROLL, ACT_HIT, ACT_KNOCKDOWN, ACT_KO, ACT_VICTORY, ACT_COUNT
} FighterAction;

/* Attack height decides how it can be blocked or dodged. */
typedef enum { HIT_HIGH, HIT_MID, HIT_LOW } HitHeight;

/* One body posture. Limb angles are absolute: 0 points straight down,
 * +90 points forward, 180 points up. [0] is the front limb, [1] the back. */
typedef struct {
    float lean, head;           // Torso lean from vertical and extra head tilt (+ = forward).
    float arm[2][2];            // [limb][0 upper arm, 1 forearm]
    float leg[2][2];            // [limb][0 thigh, 1 shin]
    float spin, rise;           // Whole-body rotation (rolls/falls) and extra hip lift in px.
} Pose;

/* Joint positions in screen pixels after posing, mirroring and grounding. */
typedef struct {
    Vec2 hip, neck, head, shoulder;
    Vec2 elbow[2], hand[2], knee[2], ankle[2], toe[2];
    float scale;
} Skeleton;

/* Static description of one attack. */
typedef struct {
    float duration, active_start, active_end;
    float damage, push;
    HitHeight height;
    bool foot, knockdown;
} MoveInfo;

typedef struct {
    float x, height, vx, vy;    // Hip x; height of the feet above ground (+ up).
    int facing;                 // +1 faces right, -1 faces left.
    FighterAction action;
    float action_time, walk_phase;
    float stun;                 // Block-stun seconds; no new actions meanwhile.
    float flash;                // White hit-flash seconds.
    float health, health_shown, energy;
    float denied;               // Seconds to flash the energy bar after a refused special.
    uint8_t buffered;           // Remembered button (KEY_*), or 0.
    float buffer_time;
    bool hit_landed;            // The current attack already connected.
} Fighter;

typedef enum { DUEL_INTRO, DUEL_FIGHT, DUEL_ROUND_END, DUEL_OVER } DuelPhase;

typedef struct {
    Fighter fighter[2];
    DuelPhase phase;
    float phase_time, clock, time_left;
    float shake, hitstop;
    float spark_time; Vec2 spark; bool spark_blocked;
    unsigned int round, rounds[2];
    int round_winner;           // 0/1, or -1 for a drawn round.
    bool knockout, over;
    int winner;                 // Match winner 0/1, or -1 for a draw.
} FighterGame;

extern const GameSpec fighter_spec;
/** Return timing/damage data for an attack action, or NULL for non-attacks. */
const MoveInfo *fighter_move(FighterAction action);
/** Start a match: round one, full health, empty energy. */
void fighter_reset(void *state, uint32_t seed);
/** Buffer each player's newly pressed button for the next physics ticks. */
void fighter_input(void *state, const InputSnapshot *snapshot);
/** Advance the duel by one fixed physics step of dt seconds. */
void fighter_update(void *state, const InputSnapshot *snapshot, float seconds);
/** Evaluate the animated pose of a fighter at its current action time. */
void fighter_pose(const Fighter *fighter, float clock, Pose *pose);
/** Forward kinematics: place a pose's joints so its lowest point rests on the
 * ground (minus height), mirrored for facing and multiplied by scale. */
void fighter_skeleton(const Pose *pose, float x, float ground, float height,
                      int facing, float scale, Skeleton *out);
/** Return true and the strike point while an attack's active window is live. */
bool fighter_strike(const Fighter *fighter, const Skeleton *skeleton, Vec2 *point);
/** Return true when a fighter counts as crouching (high attacks miss). */
bool fighter_is_low(const Fighter *fighter);
/** Compose arena, fighters, effects and HUD into SRAM. */
void fighter_draw(const void *state);
/** Small launcher picture: one fighter landing a Brazilian kick. */
void fighter_icon(int cx, int cy);
#endif
