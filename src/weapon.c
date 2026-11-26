/*
 * FRONTIER - weapon table and projectile pool.
 *
 * Speeds are FP12 pixels per frame at 60 fps:
 *   36864 = 9 px/frame = 540 px/s     24576 = 6 px/frame     28672 = 7 px/frame
 */
#include <psxgpu.h>

#include "enemy.h"
#include "fx.h"
#include "gfx.h"
#include "weapon.h"
#include "audio.h"

const WeaponDef weapon_defs[WEAPON_COUNT] = {
    /* id kind       mount flags dmg range speed  interval energy ammo sprite price */
    {  0, WK_BOLT,   0,    0,    10, 360, 36864, 8,       3,     0,   0,     0 },   /* Laser (energy) */
    {  1, WK_ROCKET, 1,    0,    45, 520, 24576, 40,      0,     1,   1,     0 },   /* Rocket */
    {  2, WK_BOLT,   0,    0,     3, 300, 28672, 16,      0,     0,   2,     0 },   /* Drone laser */
    {  3, WK_BOLT,   0,    0,     6, 340, 32768, 20,      0,     0,   3,     0 },   /* Pirate laser */
    {  4, WK_ROCKET, 1,    0,    35, 900, 20480, 50,      0,     1,   4,     0 },   /* Seeker missile */
    {  5, WK_ROCKET, 1,    0,    50, 480, 24576, 60,      0,     1,   5,     0 },   /* Ion missile (see ship_damage) */
    {  6, WK_BOLT,   2,    0,     5, 320, 32768, 18,      2,     0,   6,     0 },   /* Laser turret */
    {  7, WK_BOLT,   0,    0,    16, 420, 28672, 22,      0,     1,   7,     0 },   /* Cannon (kinetic, 1 round) */
    {  8, WK_BOLT,   0,    0,    60, 700, 61440, 90,     25,     0,   8,     0 },   /* Railgun (energy) */
    {  9, WK_BOLT,   2,    0,    10, 380, 28672, 16,      0,     1,   9,     0 },   /* Gun turret (kinetic) */
    { 10, WK_BOLT,   0,    0,    32, 520, 32768, 34,      0,     2,  10,     0 },   /* Heavy turret (kinetic, 3x3) */
};

const WeaponRole weapon_roles[WEAPON_COUNT] = {
    { 130,  70 },   /* laser: strips shields */
    {  60, 160 },   /* rocket: wrecks hulls */
    { 100, 100 },   /* drone laser */
    { 130,  70 },   /* pirate laser */
    {  70, 140 },   /* seeker */
    { 100, 100 },   /* ion: its own rules (ship.c) */
    { 130,  70 },   /* laser turret */
    {  80, 130 },   /* cannon: kinetic, for hulls */
    { 110, 150 },   /* railgun: one heavy slug, through anything */
    {  80, 130 },   /* gun turret */
    {  90, 140 },   /* heavy turret */
};

const char *weapon_role_text(int weapon) {
    switch (weapon) {
    case WEAPON_LASER: case WEAPON_TURRET: case WEAPON_PIRATE_LASER:
        return "STRIPS SHIELDS, WEAK VS BIG";
    case WEAPON_CANNON: case WEAPON_GUN_TURRET:
        return "HULL DAMAGE, WEAK VS SHIELDS";
    case WEAPON_RAILGUN:      return "PIERCES CAPITAL SHIELDS";
    case WEAPON_HEAVY_TURRET: return "HEAVY HITS, GOOD VS BIG SHIPS";
    case WEAPON_ROCKET:       return "ROCKETS: HULL. ION: SHIELDS";
    default:                  return "";
    }
}

const u8 missile_weapon[MSL_COUNT] = { WEAPON_ROCKET, WEAPON_SEEKER, WEAPON_ION };

const char *missile_name(int type) {
    static const char *n[MSL_COUNT] = { "ROCKET", "SEEKER", "ION" };
    return n[type];
}

int proj_hit_weapon = WEAPON_NONE;
int proj_spawn_owner, proj_hit_owner;
int proj_spawn_tier = 1;

#define SEEKER_STEER_SHIFT 3      /* velocity eases 1/8 of the way toward the target each frame */

_Static_assert(sizeof(WeaponDef) == 16, "WeaponDef must stay 16 bytes");

Projectile proj_pool[MAX_PROJECTILES];
static int active_count;
static int peak_count;

void proj_init(void) {
    for (int i = 0; i < MAX_PROJECTILES; i++) proj_pool[i].active = 0;
    active_count = 0;
    peak_count = 0;
}

static Projectile *spawn(s32 x, s32 y, s32 vx, s32 vy, int weapon, int team) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *p = &proj_pool[i];
        if (p->active) continue;
        const WeaponDef *w = &weapon_defs[weapon];
        p->x = x;  p->y = y;
        p->vx = vx;  p->vy = vy;
        p->life = (s16) (((s32) w->range << FP_SHIFT) / w->proj_speed);
        p->weapon = (u8) weapon;
        p->team = (u8) team;
        p->active = 1;
        p->homing = 255;
        p->owner = (u8) proj_spawn_owner;
        p->tier = (u8) proj_spawn_tier;
        return p;
    }
    return 0;
}

/* The sound of a shot (M19a): yours and your wing's sound brighter than theirs. */
static void shot_sound(s32 x, s32 y, int weapon, int team) {
    int id;
    switch (weapon) {
    case WEAPON_ROCKET: case WEAPON_SEEKER: case WEAPON_ION: id = SFX_MISSILE; break;
    case WEAPON_CANNON: case WEAPON_RAILGUN: case WEAPON_GUN_TURRET: case WEAPON_HEAVY_TURRET: id = SFX_CANNON; break;
    default:
        if (proj_spawn_owner > 0 && enemies_is_alien(proj_spawn_owner - 1)) id = SFX_VEYR_SHOT;
        else id = (team == TEAM_PLAYER || team == TEAM_ALLY) ? SFX_LASER : SFX_LASER_ENEMY;
    }
    audio_sfx_world(id, (int) FROM_FP(x), (int) FROM_FP(y));
}

int proj_spawn(s32 x, s32 y, s32 vx, s32 vy, int weapon, int team) {
    int ok = spawn(x, y, vx, vy, weapon, team) != 0;
    if (ok) shot_sound(x, y, weapon, team);
    return ok;
}

int proj_spawn_homing(s32 x, s32 y, s32 vx, s32 vy, int weapon, int team, int target) {
    Projectile *p = spawn(x, y, vx, vy, weapon, team);
    if (!p) return 0;
    shot_sound(x, y, weapon, team);
    p->homing = (u8) target;
    return 1;
}

/* Turns a seeker toward its target, keeping its speed. */
static void steer(Projectile *p) {
    const Ship *t = enemies_ship(p->homing);
    if (!t || ship_disabled(t)) { p->homing = 255; return; }
    s32 speed = weapon_defs[p->weapon].proj_speed;
    s32 dx = FROM_FP(t->x - p->x), dy = FROM_FP(t->y - p->y);
    s32 d = approx_len(dx, dy);
    if (d < 1) return;
    s32 wx = dx * speed / d, wy = dy * speed / d;          /* wanted velocity */
    p->vx += (wx - p->vx) >> SEEKER_STEER_SHIFT;
    p->vy += (wy - p->vy) >> SEEKER_STEER_SHIFT;
    s32 v = approx_len(p->vx, p->vy);
    if (v > 0) { p->vx = p->vx * speed / v; p->vy = p->vy * speed / v; }
}

void proj_update(int frame) {
    int n = 0;
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *p = &proj_pool[i];
        if (!p->active) continue;

        if (p->homing != 255) steer(p);
        p->x += p->vx;
        p->y += p->vy;

        if (weapon_defs[p->weapon].kind == WK_ROCKET && (frame & 1))
            fx_spawn(p->x, p->y, -(p->vx >> 3), -(p->vy >> 3), 14, FX_TRAIL);

        if (--p->life <= 0) {
            if (weapon_defs[p->weapon].kind == WK_ROCKET)
                fx_burst(p->x, p->y, 6, 6000, 18, FX_FIRE);   /* fizzles out */
            p->active = 0;
            continue;
        }
        n++;
    }
    active_count = n;
    if (n > peak_count) peak_count = n;
}

int  proj_count(void)      { return active_count; }
int  proj_peak(void)       { return peak_count; }
void proj_reset_peak(void) { peak_count = active_count; }

void proj_draw(s32 cam_x, s32 cam_y) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        const Projectile *p = &proj_pool[i];
        if (!p->active) continue;

        int sx = SCREEN_W / 2 + FROM_FP(p->x - cam_x);
        int sy = SCREEN_H / 2 + FROM_FP(p->y - cam_y);
        if (sx < -40 || sx > SCREEN_W + 40 || sy < -40 || sy > SCREEN_H + 40)
            continue;

        int kinetic = p->weapon == WEAPON_CANNON || p->weapon == WEAPON_GUN_TURRET || p->weapon == WEAPON_HEAVY_TURRET;
        int missile = weapon_defs[p->weapon].kind == WK_ROCKET;
        u32 c;
        if (p->weapon == WEAPON_ROCKET)           c = 0xFFBE46;
        else if (p->weapon == WEAPON_SEEKER)      c = 0xFFF05A;
        else if (p->weapon == WEAPON_ION)         c = 0x6EC8FF;
        else if (kinetic)                         c = 0xFFDC8C;
        else if (p->weapon == WEAPON_RAILGUN)     c = 0xC8E6FF;
        else if (p->team == TEAM_PLAYER || p->team == TEAM_ALLY) c = 0x64FF96;
        else if (p->team == TEAM_FED)             c = 0x78BEFF;
        else if (p->team == TEAM_REBEL)           c = 0xF06EDC;
        else                                      c = 0xFF5A5A;
        int r = (int) (c >> 16 & 255), g = (int) (c >> 8 & 255), b = (int) (c & 255);

        /* A bolt: white-hot at the head, its colour fading along the tail, with a
         * halo of light (energy weapons and missiles; slugs are just tracers). */
        int len = missile ? 3 : kinetic ? 2 : p->weapon == WEAPON_RAILGUN ? 5 : 3;
        int tx = sx - FROM_FP(p->vx * len) / 2, ty = sy - FROM_FP(p->vy * len) / 2;
        u32 tail = (u32) (r / 5) << 16 | (u32) (g / 5) << 8 | (u32) (b / 5);
        gfx_line(sx, sy, tx, ty, LAYER_PROJECTILE, kinetic ? c : 0xFFFFFF, tail, 1);
        gfx_line(sx + 1, sy, tx + 1, ty, LAYER_PROJECTILE, c, tail, 1);
        gfx_line(sx, sy + 1, tx, ty + 1, LAYER_PROJECTILE, c, tail, 1);
        if (missile) {
            gfx_glow(sx, sy, 6, 6, LAYER_PROJECTILE, 255, 255, 255);
            gfx_glow(tx, ty, 11, 11, LAYER_PROJECTILE, r, g / 2, b / 4);       /* the motor */
        } else if (!kinetic) {
            int gr = p->weapon == WEAPON_RAILGUN ? 12 : 9;
            gfx_glow(sx, sy, gr, gr, LAYER_PROJECTILE, r, g, b);
        }
    }
}
