#include <psxgpu.h>
#include <psxgte.h>

#include "collide.h"
#include "drone.h"
#include "fx.h"
#include "gfx.h"
#include "rng.h"
#include "weapon.h"

#define DRONE_HULL          40
#define DRONE_RESPAWN       150    /* frames until a destroyed drone returns */
#define STRESS_FIRE_PERIOD  12     /* frames between shots per drone in stress mode */

enum { DRONE_UNUSED = 0, DRONE_ALIVE, DRONE_DEAD };

typedef struct {
    s32 x, y;           /* FP12 world pixels */
    s32 vx, vy;         /* FP12 pixels per frame */
    s16 hull;
    s16 timer;          /* respawn countdown (dead) or fire countdown (alive) */
    u8  state;
    u8  flash;          /* frames of hit flash left */
    u8  pad0, pad1;
} Drone;

static Drone drones[MAX_DRONES];
static int target_count = DRONE_NORMAL_COUNT;
static int kills;
static int alive_count;

/* Puts a drone on a ring around the player with a random drift. */
static void drone_place(Drone *d, s32 px, s32 py, int stress) {
    int min_r = stress ? 60  : 250;
    int max_r = stress ? 200 : 450;
    int r = min_r + rng_range(max_r - min_r + 1);
    int a = rng_next() & ANGLE_MASK;

    d->x = px + isin(a) * r;          /* isin is FP12, r is pixels: result FP12 */
    d->y = py - icos(a) * r;
    d->vx = rng_range(4097) - 2048;   /* +-0.5 pixel per frame */
    d->vy = rng_range(4097) - 2048;
    d->hull = DRONE_HULL;
    d->flash = 0;
    d->state = DRONE_ALIVE;
    d->timer = 30 + rng_range(60);
}

void drones_init(void) {
    for (int i = 0; i < MAX_DRONES; i++) drones[i].state = DRONE_UNUSED;
    kills = 0;
    alive_count = 0;
    target_count = DRONE_NORMAL_COUNT;
}

void drones_set_mode(int stress, s32 px, s32 py) {
    target_count = stress ? MAX_DRONES : DRONE_NORMAL_COUNT;
    for (int i = 0; i < MAX_DRONES; i++) {
        if (i < target_count) drone_place(&drones[i], px, py, stress);
        else                  drones[i].state = DRONE_UNUSED;
    }
}

int drones_alive(void)  { return alive_count; }
int drones_target(void) { return target_count; }
int drones_kills(void)  { return kills; }

void drones_update(s32 px, s32 py, int stress) {
    int leash = stress ? 260 : 600;
    int n = 0;

    for (int i = 0; i < MAX_DRONES; i++) {
        Drone *d = &drones[i];
        if (d->state == DRONE_UNUSED) continue;

        if (d->state == DRONE_DEAD) {
            if (--d->timer <= 0) drone_place(d, px, py, stress);
            continue;
        }
        n++;

        d->x += d->vx;
        d->y += d->vy;
        if (d->flash) d->flash--;

        /* Too far from the player: come back from the other side. */
        int dxp = FROM_FP(d->x - px), dyp = FROM_FP(d->y - py);
        int dist = (int) approx_len(dxp, dyp);
        if (dist > leash) {
            drone_place(d, px, py, stress);
            continue;
        }

        if (--d->timer > 0) continue;

        if (stress) {
            /* Shoot in a random direction: this exists to fill the projectile pool. */
            const WeaponDef *w = &weapon_defs[WEAPON_DRONE_LASER];
            int a = rng_next() & ANGLE_MASK;
            s32 vx = (isin(a) * (s32) w->proj_speed) >> FP_SHIFT;
            s32 vy = (-icos(a) * (s32) w->proj_speed) >> FP_SHIFT;
            proj_spawn(d->x, d->y, vx, vy, WEAPON_DRONE_LASER, TEAM_ENEMY);
            d->timer = STRESS_FIRE_PERIOD;
        } else {
            d->timer = 70;   /* normal mode: passive targets; the pirates are the threat */
        }
    }
    alive_count = n;
}

void drones_add_hittables(void) {
    for (int i = 0; i < MAX_DRONES; i++) {
        const Drone *d = &drones[i];
        if (d->state == DRONE_ALIVE)
            collide_add(FROM_FP(d->x), FROM_FP(d->y), DRONE_RADIUS, TEAM_ENEMY, i);
    }
}

void drones_damage(int id, int damage) {
    if (id < 0 || id >= MAX_DRONES) return;
    Drone *d = &drones[id];
    if (d->state != DRONE_ALIVE) return;

    d->hull -= damage;
    d->flash = 3;
    if (d->hull > 0) return;

    d->state = DRONE_DEAD;
    d->timer = DRONE_RESPAWN;
    kills++;
    fx_burst(d->x, d->y, 18, 10000, 28, FX_FIRE);
    fx_burst(d->x, d->y,  8, 14000, 18, FX_SPARK);
}

void drones_draw(s32 cam_x, s32 cam_y) {
    for (int i = 0; i < MAX_DRONES; i++) {
        const Drone *d = &drones[i];
        if (d->state != DRONE_ALIVE) continue;

        int cx = SCREEN_W / 2 + FROM_FP(d->x - cam_x);
        int cy = SCREEN_H / 2 + FROM_FP(d->y - cam_y);
        if (cx < -16 || cx > SCREEN_W + 16 || cy < -16 || cy > SCREEN_H + 16)
            continue;

        POLY_F4 *q = (POLY_F4 *) gfx_prim(LAYER_SHIP, sizeof(POLY_F4));
        if (!q) return;
        setPolyF4(q);
        /* Vertex order for a quad: top, right, left, bottom. */
        setXY4(q, cx, cy - DRONE_RADIUS,
                  cx + DRONE_RADIUS, cy,
                  cx - DRONE_RADIUS, cy,
                  cx, cy + DRONE_RADIUS);
        if (d->flash) setRGB0(q, 255, 255, 255);
        else          setRGB0(q, 200, 70, 200);
    }
}
