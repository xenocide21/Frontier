#include <psxgpu.h>
#include <psxgte.h>

#include "fx.h"
#include "gfx.h"
#include "rng.h"

static Particle parts[MAX_PARTICLES];
static int cursor;
static int active_count;

void fx_init(void) {
    for (int i = 0; i < MAX_PARTICLES; i++) parts[i].life = 0;
    cursor = 0;
    active_count = 0;
}

void fx_spawn(s32 x, s32 y, int vx, int vy, int life, int kind) {
    if (life > 31) life = 31;
    if (life < 1)  life = 1;

    /* Take the next free slot; if every slot is busy, overwrite one. */
    int slot = cursor;
    for (int tries = 0; tries < MAX_PARTICLES; tries++) {
        int i = (cursor + tries) % MAX_PARTICLES;
        if (parts[i].life == 0) { slot = i; break; }
    }
    cursor = (slot + 1) % MAX_PARTICLES;

    Particle *p = &parts[slot];
    p->x = x;  p->y = y;
    p->vx = (s16) vx;  p->vy = (s16) vy;
    p->life = (u8) life;
    p->kind = (u8) kind;
}

void fx_burst(s32 x, s32 y, int count, int speed, int life, int kind) {
    for (int i = 0; i < count; i++) {
        int a = rng_next() & ANGLE_MASK;
        int s = (speed >> 2) + rng_range(speed - (speed >> 2) + 1);
        int vx = (isin(a) * s) >> FP_SHIFT;
        int vy = (-icos(a) * s) >> FP_SHIFT;
        fx_spawn(x, y, vx, vy, life - rng_range(life / 3 + 1), kind);
    }
}

void fx_update(void) {
    int n = 0;
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &parts[i];
        if (!p->life) continue;
        p->x += p->vx;
        p->y += p->vy;
        p->vx -= p->vx >> 4;    /* a little drag */
        p->vy -= p->vy >> 4;
        p->life--;
        if (p->life) n++;
    }
    active_count = n;
}

int fx_count(void) { return active_count; }

void fx_draw(s32 cam_x, s32 cam_y) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        const Particle *p = &parts[i];
        if (!p->life) continue;

        int sx = SCREEN_W / 2 + FROM_FP(p->x - cam_x);
        int sy = SCREEN_H / 2 + FROM_FP(p->y - cam_y);
        if (sx < -8 || sx > SCREEN_W + 8 || sy < -8 || sy > SCREEN_H + 8) continue;

        int f = p->life * 8;          /* brightness 8..248 */
        int r, g, b, w;
        switch (p->kind) {
        case FX_FIRE:
            w = 1 + (p->life >> 3);   /* shrinks as it fades */
            r = f;
            g = (p->life > 14) ? (f * 5) >> 3 : f >> 2;
            b = f >> 4;
            break;
        case FX_TRAIL:
            w = 2;
            r = f;  g = f >> 1;  b = 0;
            break;
        default:                      /* FX_SPARK */
            w = 2;
            r = f;  g = f;  b = f >> 1;
            break;
        }

        /* Each particle is a small core with light around it; fire blooms as it dies. */
        int halo = p->kind == FX_FIRE ? 5 + ((32 - (p->life > 32 ? 32 : p->life)) >> 2) + w * 2 : p->kind == FX_TRAIL ? 6 : 5;
        gfx_glow(sx, sy, halo, halo, LAYER_PARTICLES, r, g, b);
        if (p->kind != FX_TRAIL) gfx_rect(sx - (w >> 1), sy - (w >> 1), w, w, LAYER_PARTICLES, r, g, b);
    }
}
