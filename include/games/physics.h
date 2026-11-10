/* Portable screen-space vectors and collision helpers; units are pixels/seconds. */
#ifndef CONSOLE_PHYSICS_H
#define CONSOLE_PHYSICS_H
#include <stdbool.h>
typedef struct { float x,y; } Vec2;
typedef struct { float x,y,w,h; } Rect;
typedef struct { Vec2 position, velocity; float radius, inverse_mass; } Body;
typedef struct { float time; Vec2 normal; } SweepHit;
/** Clamp a finite value to inclusive bounds. */
float physics_clamp(float value, float low, float high);
/** Return Euclidean vector length. */
float physics_length(Vec2 value);
/** Return a unit vector, or zero for a near-zero vector. */
Vec2 physics_unit(Vec2 value);
/** Cap vector magnitude while preserving direction. */
Vec2 physics_limit(Vec2 value, float maximum);
/** Resolve overlapping circles in RAM; zero inverse mass is immovable. */
bool physics_circle_pair(Body *a, Body *b, float restitution);
/** Sweep a circle by relative displacement against a stationary rectangle, including corners. */
bool physics_sweep_rect(Vec2 start, Vec2 delta, float radius, Rect box, SweepHit *hit);
#endif
