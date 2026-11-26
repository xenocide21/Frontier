/*
 * FRONTIER - particles: sparks, fire and engine/rocket trails.
 *
 * 16-byte particles in a fixed pool of MAX_PARTICLES. Lifetimes are at most
 * 31 frames so a particle's brightness is simply life * 8.
 */
#ifndef FRONTIER_FX_H
#define FRONTIER_FX_H

#include "common.h"

#define MAX_PARTICLES 96

enum { FX_SPARK = 0, FX_FIRE, FX_TRAIL };

typedef struct {
    s32 x, y;           /* FP12 world pixels */
    s16 vx, vy;         /* FP12 pixels per frame */
    u8  life;           /* 0 = free slot */
    u8  kind;
    u8  pad0, pad1;
} Particle;

void fx_init(void);
void fx_spawn(s32 x, s32 y, int vx, int vy, int life, int kind);

/* Burst of 'count' particles flying out in random directions. speed is FP12
 * pixels per frame (maximum). */
void fx_burst(s32 x, s32 y, int count, int speed, int life, int kind);

void fx_update(void);
void fx_draw(s32 cam_x, s32 cam_y);
int  fx_count(void);

#endif
