/* Collision mathematics shared by host tests and embedded games; no hardware. */
#include <math.h>
#include "games/physics.h"

/** Return finite value constrained to [low,high]; callers supply ordered bounds. */
float physics_clamp(float value, float low, float high) { return fmaxf(low,fminf(value,high)); }

/** Return the length in the vector's own units; no state changes. */
float physics_length(Vec2 value) { return sqrtf(value.x*value.x+value.y*value.y); }

/** Normalize a vector without dividing by zero. */
Vec2 physics_unit(Vec2 value) {
    float length=physics_length(value);
    return length>0.00001f?(Vec2){value.x/length,value.y/length}:(Vec2){0,0};
}

/** Limit vector magnitude to positive maximum, preserving smaller vectors. */
Vec2 physics_limit(Vec2 value, float maximum) {
    float length=physics_length(value);
    if (length>maximum && length>0) { value.x*=maximum/length; value.y*=maximum/length; }
    return value;
}

/** Resolve circle penetration and apply an impulse only while surfaces approach.
 * @param a,b Mutable bodies with nonnegative inverse masses and positive radii.
 * @param restitution Bounce coefficient 0..1. No hardware or timing is involved.
 * @return Whether the circles overlapped; two immovable bodies are ignored.
 */
bool physics_circle_pair(Body *a, Body *b, float restitution) {
    Vec2 delta={b->position.x-a->position.x,b->position.y-a->position.y};
    float radius=a->radius+b->radius, squared=delta.x*delta.x+delta.y*delta.y;
    float mass=a->inverse_mass+b->inverse_mass;
    if (squared>=radius*radius || mass<=0) return false;
    float distance=sqrtf(squared);
    Vec2 n=distance>0.00001f?(Vec2){delta.x/distance,delta.y/distance}:(Vec2){1,0};
    float depth=(radius-distance+0.001f)/mass;
    a->position.x-=n.x*depth*a->inverse_mass; a->position.y-=n.y*depth*a->inverse_mass;
    b->position.x+=n.x*depth*b->inverse_mass; b->position.y+=n.y*depth*b->inverse_mass;
    float closing=(b->velocity.x-a->velocity.x)*n.x+(b->velocity.y-a->velocity.y)*n.y;
    if (closing<0) {
        float impulse=-(1+restitution)*closing/mass;
        a->velocity.x-=impulse*a->inverse_mass*n.x; a->velocity.y-=impulse*a->inverse_mass*n.y;
        b->velocity.x+=impulse*b->inverse_mass*n.x; b->velocity.y+=impulse*b->inverse_mass*n.y;
    }
    return true;
}

/** Keep the earliest normalized contact in [0,1] when displacement approaches its normal. */
static void consider_hit(float time, Vec2 normal, Vec2 delta, SweepHit *hit) {
    if (time>=-0.00001f && time<=1 && time<hit->time &&
        delta.x*normal.x+delta.y*normal.y < -0.000001f) {
        hit->time=fmaxf(0,time); hit->normal=normal;
    }
}

/** Test the exact rounded rectangle boundary for a moving circle.
 * @param delta Circle displacement relative to rectangle motion over the interval.
 * @param hit Output earliest fraction and outward normal; radius must be positive.
 * @return True for approaching contact; moving-away touches are not repeated hits.
 */
bool physics_sweep_rect(Vec2 start, Vec2 delta, float radius, Rect box, SweepHit *hit) {
    hit->time=2;
    Vec2 closest={physics_clamp(start.x,box.x,box.x+box.w),physics_clamp(start.y,box.y,box.y+box.h)};
    Vec2 offset={start.x-closest.x,start.y-closest.y};
    float distance=physics_length(offset);
    if (distance<radius-0.00001f) {
        Vec2 normal;
        if (distance>0.00001f) normal=(Vec2){offset.x/distance,offset.y/distance};
        else {
            float gaps[]={start.x-box.x,box.x+box.w-start.x,start.y-box.y,box.y+box.h-start.y};
            Vec2 normals[]={{-1,0},{1,0},{0,-1},{0,1}};
            unsigned int nearest=0;
            for (unsigned int i=1;i<4;i++) if (gaps[i]<gaps[nearest]) nearest=i;
            normal=normals[nearest];
        }
        consider_hit(0,normal,delta,hit);
    }
    if (fabsf(delta.x)>0.000001f) {
        float plane=delta.x>0?box.x-radius:box.x+box.w+radius;
        float time=(plane-start.x)/delta.x, y=start.y+time*delta.y;
        if (y>=box.y && y<=box.y+box.h) consider_hit(time,(Vec2){delta.x>0?-1:1,0},delta,hit);
    }
    if (fabsf(delta.y)>0.000001f) {
        float plane=delta.y>0?box.y-radius:box.y+box.h+radius;
        float time=(plane-start.y)/delta.y, x=start.x+time*delta.x;
        if (x>=box.x && x<=box.x+box.w) consider_hit(time,(Vec2){0,delta.y>0?-1:1},delta,hit);
    }
    float a=delta.x*delta.x+delta.y*delta.y;
    if (a>0.000001f) {
        for (unsigned int cx=0;cx<2;cx++) for (unsigned int cy=0;cy<2;cy++) {
            Vec2 corner={box.x+cx*box.w,box.y+cy*box.h};
            Vec2 relative={start.x-corner.x,start.y-corner.y};
            float b=2*(relative.x*delta.x+relative.y*delta.y);
            float c=relative.x*relative.x+relative.y*relative.y-radius*radius;
            float discriminant=b*b-4*a*c;
            if (discriminant<0) continue;
            float time=(-b-sqrtf(discriminant))/(2*a);
            Vec2 point={start.x+delta.x*time,start.y+delta.y*time};
            if ((cx?point.x>=corner.x:point.x<=corner.x) && (cy?point.y>=corner.y:point.y<=corner.y)) {
                Vec2 normal=physics_unit((Vec2){point.x-corner.x,point.y-corner.y});
                consider_hit(time,normal,delta,hit);
            }
        }
    }
    return hit->time<=1;
}
