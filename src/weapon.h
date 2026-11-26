/*
 * FRONTIER - weapons and projectiles.
 *
 * WeaponDef is the 16-byte record from the technical design. Projectiles live
 * in one fixed pool of MAX_PROJECTILES; when the pool is full a new shot is
 * simply not fired (the HUD shows the peak so the limit can be tuned).
 */
#ifndef FRONTIER_WEAPON_H
#define FRONTIER_WEAPON_H

#include "common.h"

enum { WEAPON_LASER = 0, WEAPON_ROCKET, WEAPON_DRONE_LASER, WEAPON_PIRATE_LASER,
       WEAPON_SEEKER, WEAPON_ION, WEAPON_TURRET, WEAPON_CANNON, WEAPON_RAILGUN, WEAPON_GUN_TURRET, WEAPON_HEAVY_TURRET,
       WEAPON_COUNT, WEAPON_NONE = 255 };

/* Missile types (L1 cycles, L2 fires), Milestone 12d:
 *   ROCKET  fires straight ahead, strong hull damage
 *   SEEKER  homes on your combat target (if you had one when it launched)
 *   ION     no hull damage: drains every shield arc and hurts engines and weapons,
 *           so it disables a ship without wrecking it (good before boarding) */
enum { MSL_ROCKET = 0, MSL_SEEKER, MSL_ION, MSL_COUNT };
extern const u8 missile_weapon[MSL_COUNT];          /* MSL_* -> WEAPON_* */
const char *missile_name(int type);

/* Fixed guns swing within this half-angle (4096 = a full turn) to hit the
 * current target; outside it they fire straight ahead. 341 = 30 degrees each
 * side, a 60 degree arc. Turrets fire all round. */
#define GUN_HALF_ARC 341
enum { WK_BOLT = 0, WK_ROCKET };
enum { TEAM_PLAYER = 0, TEAM_ENEMY = 1, TEAM_MERCHANT = 2, TEAM_FED = 3, TEAM_REBEL = 4, TEAM_ALLY = 5 };   /* ALLY: your escorts, on the player's side */

/* 16 bytes. proj_speed is FP12 pixels per frame; range is pixels. */
typedef struct {
    u8  id, kind, mount_type, flags;
    u16 damage;
    u16 range;
    u16 proj_speed;
    u8  fire_interval;      /* frames between shots */
    u8  energy_cost;        /* not used yet */
    u8  ammo_per_shot;
    u8  sprite_id;          /* not used yet */
    u16 price;              /* not used yet */
} WeaponDef;

extern const WeaponDef weapon_defs[WEAPON_COUNT];

/* Weapon roles (Milestone 13f): how hard each weapon hits shields and hull,
 * in percent of its damage. Lasers strip shields, guns and missiles wreck hulls.
 * Capital ships' shields also turn aside part of every hit (see ship.c), so a
 * swarm of light lasers barely scratches a frigate: bring railguns, heavy
 * turrets or missiles. */
typedef struct { u8 shield_pct, hull_pct; } WeaponRole;
extern const WeaponRole weapon_roles[WEAPON_COUNT];
const char *weapon_role_text(int weapon);

#define MAX_PROJECTILES 128

typedef struct {
    s32 x, y;               /* FP12 world pixels */
    s32 vx, vy;             /* FP12 pixels per frame */
    s16 life;               /* frames left */
    u8  weapon, team;
    u8  active;
    u8  homing;             /* seekers: enemy slot to home on, or 255 */
    u8  owner;              /* NPC shots: enemy slot + 1 of the shooter, 0 = player/other */
    u8  tier;               /* module tier of the gun that fired it: damage x1, x1.4, x1.9 */
} Projectile;

/* The weapon of the projectile currently being resolved by the collision code
 * (WEAPON_NONE outside a hit). ship_damage reads it for ion hits. */
extern int proj_hit_weapon;
/* Set to enemy slot + 1 while an NPC fires (0 otherwise); stamped on its shots.
 * proj_hit_owner holds the stamp of the shot being resolved by a hit. */
extern int proj_spawn_owner, proj_hit_owner;
extern int proj_spawn_tier;      /* set while a mount fires (1-3) */

extern Projectile proj_pool[MAX_PROJECTILES];

void proj_init(void);

/* Fires a projectile. vx, vy are the final velocity (ship velocity included).
 * Range is measured from the muzzle at the weapon's own speed. Returns 0 if
 * the pool is full. */
int  proj_spawn(s32 x, s32 y, s32 vx, s32 vy, int weapon, int team);
/* Same, for a seeker that homes on enemy slot `target` (or 255 for none). */
int  proj_spawn_homing(s32 x, s32 y, s32 vx, s32 vy, int weapon, int team, int target);

void proj_update(int frame);
void proj_draw(s32 cam_x, s32 cam_y);

int  proj_count(void);      /* active now */
int  proj_peak(void);       /* highest count seen since the last reset */
void proj_reset_peak(void);

#endif
