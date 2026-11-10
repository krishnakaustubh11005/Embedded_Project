/* Shade Duel animation: keyframed joint angles and forward kinematics.
 *
 * Instead of storing bitmap frames, every move is a short list of poses
 * (joint angles) at given times. Between keys the angles are blended with a
 * smooth ease, then forward kinematics turns angles into joint positions:
 * each bone starts where its parent ends. A whole move costs ~50 bytes per key
 * in flash, versus ~20 KB for a single 100x100 RGB565 sprite frame.
 */
#include <math.h>
#include "games/fighter/fighter.h"

/* Bone lengths in pixels at scale 1. A standing fighter is about 72 px tall. */
enum { THIGH=17, SHIN=17, TORSO=22, SHOULDER=19, UPPER_ARM=13, FOREARM=13, FOOT=5, HEAD_OFFSET=8 };
#define HEAD_RADIUS 6.5f

typedef struct { float time; Pose pose; } PoseKey;

/* Pose constructor: lean, head, front arm, back arm, front leg, back leg, spin, rise. */
#define POSE(l,h,fa0,fa1,ba0,ba1,fl0,fl1,bl0,bl1,s,r) \
    {l,h,{{fa0,fa1},{ba0,ba1}},{{fl0,fl1},{bl0,bl1}},s,r}

/* Fists up near the chin, front foot forward, knees soft. */
#define GUARD        POSE(  8,  0,  35,150,  15,140,  18,  5, -18,-22,    0,0)
#define CROUCH       POSE( 25,  0,  45,150,  25,140,  85,-40,  35,-100,   0,0)
#define TUCK         POSE( 60, 20, 120,170, 100,160, 130,-20, 110,-30,    0,0)
#define HURT         POSE(-25,-20,  20, 60, -30, 30,  25, 10, -25,-30,    0,0)
#define LYING        POSE(  0,  0, 160,170, 150,165,   5,  0,  -5,  0,  -90,0)
#define BLOCKING     POSE(  0, -5,  70,175,  50,170,  18,  5, -18,-22,    0,0)
#define LOW_BLOCKING POSE( 20, -5,  75,175,  55,170,  85,-40,  35,-100,   0,0)
#define AIRBORNE     POSE(  5,  0,  60,150,  30,140,  60,-30,  20,-70,    0,0)

static const PoseKey guard_keys[]={{0,GUARD}};
static const PoseKey crouch_keys[]={{0,CROUCH}};
static const PoseKey block_keys[]={{0,BLOCKING}};
static const PoseKey low_block_keys[]={{0,LOW_BLOCKING}};
static const PoseKey jump_keys[]={{0,AIRBORNE}};

/* A: quick jab with the front fist, straight out at head height. */
static const PoseKey punch_keys[]={
    {0.00f,GUARD},
    {0.07f,POSE(14,0,  95, 98,   5,150,  22,  5, -20,-22, 0,0)},
    {0.17f,POSE(14,0,  95, 98,   5,150,  22,  5, -20,-22, 0,0)},
    {0.30f,GUARD}};

/* Forward+A: dip the knees, then drive the fist upward while stepping in. */
static const PoseKey uppercut_keys[]={
    {0.00f,GUARD},
    {0.12f,POSE( 22,  8,  10, 70,  20,140,  40,-10, -20,-40, 0,0)},
    {0.20f,POSE( -6,-10, 130,160,   0,120,  10,  0, -15,-15, 0,3)},
    {0.32f,POSE( -6,-10, 130,160,   0,120,  10,  0, -15,-15, 0,3)},
    {0.50f,GUARD}};

/* B: chamber the knee, snap the front foot out at hip height. */
static const PoseKey kick_keys[]={
    {0.00f,GUARD},
    {0.12f,POSE(  0, 0,  40,150, -10,120,  80,-10, -15,-15, 0,0)},
    {0.20f,POSE(-18,-5,  50,150, -20,110,  88, 92, -12, -8, 0,0)},
    {0.28f,POSE(-18,-5,  50,150, -20,110,  88, 92, -12, -8, 0,0)},
    {0.42f,GUARD}};

/* Down+B: drop onto the back knee and scythe the front leg along the floor. */
static const PoseKey sweep_keys[]={
    {0.00f,GUARD},
    {0.14f,CROUCH},
    {0.22f,POSE( 35, 5,  60,120,  30,100,  86, 92,  65,-40, 0,0)},
    {0.32f,POSE( 35, 5,  60,120,  30,100,  86, 92,  65,-40, 0,0)},
    {0.45f,CROUCH},
    {0.55f,GUARD}};

/* Airborne B: flying side kick, front leg extended down-forward. */
static const PoseKey fly_kick_keys[]={
    {0.00f,AIRBORNE},
    {0.08f,POSE(-10,0,  60,140, -30, 30,  62, 70,  40,-60, 0,0)}};

/* D: Brazilian (question-mark) kick. The knee rises as if for a front kick,
 * the hips turn over and lean back, the foot climbs above head height, then
 * the shin whips down and forward onto the opponent's head. */
static const PoseKey brazil_keys[]={
    {0.00f,GUARD},
    {0.14f,POSE( -5,  0,  60,130, -30, 20, 110, 40,  -10,-10, 0,0)},  // Knee chambers high.
    {0.26f,POSE(-35,-10,  70,140, -50,  0, 160,175,   -5, -5, 0,4)},  // Foot climbs overhead.
    {0.36f,POSE(-30, -5,  60,140, -40, 10, 140,105,   -5, -5, 0,4)},  // Shin whips down (strike).
    {0.46f,POSE(-15,  0,  50,140, -20, 60, 100, 75,  -10,-10, 0,2)},  // Follow-through.
    {0.72f,GUARD}};

/* Back+C: backward roll. Spin -360 rotates the tucked body once; the final
 * key keeps -360 (identical to 0) so the blend does not unwind the turn. */
static const PoseKey roll_keys[]={
    {0.00f,CROUCH},
    {0.06f,TUCK},
    {0.34f,POSE(60,20, 120,170, 100,160, 130,-20, 110,-30, -360,0)},
    {0.42f,POSE(25, 0,  45,150,  25,140,  85,-40,  35,-100,-360,0)}};

static const PoseKey hit_keys[]={{0.00f,GUARD},{0.05f,HURT},{0.28f,GUARD}};

/* Fall onto the back, lie still, sit up and stand. */
static const PoseKey knockdown_keys[]={
    {0.00f,HURT},
    {0.35f,LYING},
    {0.90f,LYING},
    {1.10f,POSE(40,10,  60,100,  20, 60,  90,  0,  90,-10, -40,0)},
    {1.30f,CROUCH},
    {1.45f,GUARD}};

static const PoseKey ko_keys[]={{0.00f,HURT},{0.35f,LYING}};

static const PoseKey victory_keys[]={
    {0.00f,GUARD},
    {0.25f,POSE(0,-10, 170,178,  30,-40,  10,  0, -10,  0, 0,0)}};

typedef struct { const PoseKey *keys; unsigned int count; } Track;
#define TRACK(k) {k,sizeof(k)/sizeof(k[0])}
static const Track tracks[ACT_COUNT]={
    [ACT_IDLE]=TRACK(guard_keys), [ACT_WALK]=TRACK(guard_keys),
    [ACT_CROUCH]=TRACK(crouch_keys), [ACT_BLOCK]=TRACK(block_keys),
    [ACT_CROUCH_BLOCK]=TRACK(low_block_keys), [ACT_JUMP]=TRACK(jump_keys),
    [ACT_PUNCH]=TRACK(punch_keys), [ACT_UPPERCUT]=TRACK(uppercut_keys),
    [ACT_KICK]=TRACK(kick_keys), [ACT_SWEEP]=TRACK(sweep_keys),
    [ACT_FLY_KICK]=TRACK(fly_kick_keys), [ACT_BRAZIL]=TRACK(brazil_keys),
    [ACT_ROLL]=TRACK(roll_keys), [ACT_HIT]=TRACK(hit_keys),
    [ACT_KNOCKDOWN]=TRACK(knockdown_keys), [ACT_KO]=TRACK(ko_keys),
    [ACT_VICTORY]=TRACK(victory_keys)};

/** Linear interpolation between two numbers. */
static float lerp(float a, float b, float amount) { return a+(b-a)*amount; }

/** Blend every angle of two poses; amount 0 gives a, 1 gives b. */
static Pose blend(const Pose *a, const Pose *b, float amount) {
    Pose r;
    r.lean=lerp(a->lean,b->lean,amount); r.head=lerp(a->head,b->head,amount);
    for (unsigned int i=0;i<2;i++) for (unsigned int j=0;j<2;j++) {
        r.arm[i][j]=lerp(a->arm[i][j],b->arm[i][j],amount);
        r.leg[i][j]=lerp(a->leg[i][j],b->leg[i][j],amount);
    }
    r.spin=lerp(a->spin,b->spin,amount); r.rise=lerp(a->rise,b->rise,amount);
    return r;
}

/** Sample a key track at time t, easing each segment with smoothstep 3u^2-2u^3.
 * @note Before the first key returns the first pose; after the last, holds it.
 */
static Pose sample(const Track *track, float t) {
    if (t<=track->keys[0].time || track->count==1) return track->keys[0].pose;
    for (unsigned int i=1;i<track->count;i++) {
        const PoseKey *a=&track->keys[i-1], *b=&track->keys[i];
        if (t<b->time) {
            float u=(t-a->time)/(b->time-a->time);
            return blend(&a->pose,&b->pose,u*u*(3-2*u));
        }
    }
    return track->keys[track->count-1].pose;
}

/** Evaluate the fighter's pose: keyframes plus procedural walk, breathing and stun.
 * @param clock Game time in seconds, used for idle breathing only.
 */
void fighter_pose(const Fighter *fighter, float clock, Pose *pose) {
    FighterAction action=fighter->action<ACT_COUNT?fighter->action:ACT_IDLE;
    *pose=sample(&tracks[action],fighter->action_time);
    if (action==ACT_IDLE) {
        pose->lean+=2*sinf(clock*3);
        pose->arm[0][1]+=4*sinf(clock*3);
    } else if (action==ACT_WALK) {
        float s=sinf(fighter->walk_phase), c=cosf(fighter->walk_phase);
        pose->leg[0][0]=10+22*s; pose->leg[0][1]=pose->leg[0][0]-8-14*fmaxf(0,c);
        pose->leg[1][0]=-10-22*s; pose->leg[1][1]=pose->leg[1][0]-8-14*fmaxf(0,-c);
        pose->arm[1][0]+=6*s;
    } else if (action==ACT_JUMP && fighter->vy<0) {
        pose->leg[0][0]=35; pose->leg[0][1]=5;  // Falling: legs reach for the floor.
        pose->leg[1][0]=-10; pose->leg[1][1]=-30;
    }
    if (fighter->stun>0) pose->lean-=12*fminf(1,fighter->stun/0.16f);
}

/** Unit direction for an absolute limb angle: 0 = down, 90 = forward, 180 = up. */
static Vec2 limb_dir(float degrees) {
    float radians=degrees*0.01745329f;
    return (Vec2){sinf(radians),cosf(radians)};
}

/** Return start + length*direction. */
static Vec2 extend(Vec2 start, Vec2 direction, float length) {
    return (Vec2){start.x+direction.x*length,start.y+direction.y*length};
}

/** Build joint positions from angles, then rotate, mirror, scale and ground them.
 * @param x Hip column on screen. @param ground Floor row. @param height Feet above floor.
 * @note Works in a local frame with the hip at (0,0) facing right. The lowest
 *       body point (including limb thickness) is then placed on the floor, so
 *       crouches, kneeling sweeps and lying knockdowns all rest on the ground
 *       without hand-tuned hip heights.
 */
void fighter_skeleton(const Pose *pose, float x, float ground, float height,
                      int facing, float scale, Skeleton *out) {
    Skeleton s;
    s.hip=(Vec2){0,0};
    Vec2 up=limb_dir(180-pose->lean);           // Torso points up, tilted forward by lean.
    s.neck=extend(s.hip,up,TORSO);
    s.shoulder=extend(s.hip,up,SHOULDER);
    s.head=extend(s.neck,limb_dir(180-pose->lean-pose->head),HEAD_OFFSET);
    for (unsigned int i=0;i<2;i++) {
        s.elbow[i]=extend(s.shoulder,limb_dir(pose->arm[i][0]),UPPER_ARM);
        s.hand[i]=extend(s.elbow[i],limb_dir(pose->arm[i][1]),FOREARM);
        s.knee[i]=extend(s.hip,limb_dir(pose->leg[i][0]),THIGH);
        s.ankle[i]=extend(s.knee[i],limb_dir(pose->leg[i][1]),SHIN);
        s.toe[i]=extend(s.ankle[i],limb_dir(pose->leg[i][1]+90),FOOT);
    }
    Vec2 *points[]={&s.hip,&s.neck,&s.head,&s.shoulder,&s.elbow[0],&s.elbow[1],
                    &s.hand[0],&s.hand[1],&s.knee[0],&s.knee[1],&s.ankle[0],&s.ankle[1],
                    &s.toe[0],&s.toe[1]};
    const unsigned int count=sizeof(points)/sizeof(points[0]);
    float c=cosf(pose->spin*0.01745329f), n=sinf(pose->spin*0.01745329f);
    float lowest=-1e9f;
    for (unsigned int i=0;i<count;i++) {
        Vec2 p=*points[i];
        *points[i]=(Vec2){p.x*c-p.y*n,p.x*n+p.y*c};  // Screen-space rotation about the hip.
        float margin=points[i]==&s.head?HEAD_RADIUS:3;
        lowest=fmaxf(lowest,points[i]->y+margin);
    }
    float hip_y=ground-(lowest+pose->rise)*scale-height;
    for (unsigned int i=0;i<count;i++) {
        *points[i]=(Vec2){x+points[i]->x*facing*scale,hip_y+points[i]->y*scale};
    }
    s.scale=scale;
    *out=s;
}

/** Strike point = front fist for punches, front ankle-to-toe midpoint for kicks. */
bool fighter_strike(const Fighter *fighter, const Skeleton *skeleton, Vec2 *point) {
    const MoveInfo *move=fighter_move(fighter->action);
    if (!move || fighter->action_time<move->active_start || fighter->action_time>move->active_end) return false;
    if (move->foot) {
        *point=(Vec2){(skeleton->ankle[0].x+skeleton->toe[0].x)/2,(skeleton->ankle[0].y+skeleton->toe[0].y)/2};
    } else *point=skeleton->hand[0];
    return true;
}
