/*
 * FRONTIER - ship fitting: hull grids, modules, layouts (Milestone 13a).
 *
 * Every hull has a square grid cut from its sprite's outline (tools/shipgen/
 * hulls.py -> hulls_data.c). Squares are zoned:
 *   Z_EDGE    on the outline: hardpoints, where weapons go
 *   Z_IN      inside: reactors, shields, capacitors, armor, storage
 *   Z_ENGINE  at the rear: engines, plus warp drives, reactors and armor
 *   Z_BRIDGE  one square: the bridge (fixed; losing it will disable the ship)
 * A Layout lists the modules placed on the grid. fit_compute turns a layout into
 * the ship's numbers and its weapon mounts.
 *
 * Power: reactors make it, the bridge, engines, shields and turrets use it. The
 * spare power refills the capacitors, which energy weapons fire from. Kinetic
 * weapons fire rounds from the ammo stores. Every weapon has a facing (its
 * module's rotation plus an arc offset the player can swing) and an arc; it
 * tracks the combat target inside that arc and fires only when it can bear.
 */
#ifndef FRONTIER_FIT_H
#define FRONTIER_FIT_H

#include "common.h"

#define GRID_MAX    32
#define LAYOUT_MAX  120
#define MOUNT_MAX   32
#define ARC_STEP    128       /* arc offsets are in steps of 128 (11.25 degrees) */

enum { Z_OUT = 0, Z_IN, Z_EDGE, Z_ENGINE, Z_BRIDGE };

/* Order matches tools/shipgen/hulls.py. */
enum {
    MT_NONE = 0, MT_COCKPIT, MT_BRIDGE, MT_LIFE_SUPPORT,
    MT_LASER, MT_CANNON, MT_RAILGUN, MT_LASER_TURRET, MT_GUN_TURRET, MT_HEAVY_TURRET, MT_MISSILE_RACK,
    MT_ENGINE, MT_BIG_ENGINE, MT_HUGE_ENGINE, MT_WARP_S, MT_WARP_M, MT_WARP_L,
    MT_REACTOR, MT_BIG_REACTOR, MT_HUGE_REACTOR, MT_CAPACITOR, MT_SHIELD_GEN, MT_BIG_SHIELD_GEN,
    MT_ARMOR, MT_AMMO_STORE, MT_CARGO_BAY, MT_FUEL_CELL, MT_COUNT
};
enum { MK_BRIDGE = 0, MK_WEAPON, MK_ENGINE, MK_INTERNAL };   /* where a module may go */

typedef struct {
    const char *name;
    const char *tag;          /* two letters drawn on the grid */
    u8  w, h;
    u8  kind;                 /* MK_* */
    u8  rotates;              /* 0: long weapons keep their facing */
    u8  weapon;               /* WEAPON_* fired, or WEAPON_NONE */
    s16 half_arc;             /* weapons: half the firing arc (4096 = full turn) */
    u8  slew;                 /* weapons: turning speed of the gun, angle units per frame */
    s8  power;                /* + makes, - uses */
    u8  mass;
    u16 value;                /* thrust, shield points, energy, armor, rounds or cargo */
    u16 price;                /* credits to install (removal refunds 30%) */
    u8  hp;                   /* damage it takes before it is destroyed */
    u8  boom;                 /* blast damage when destroyed (reactors, fuel cells), else 0 */
    u8  r, g, b;              /* colour on the editor grid */
} ModuleDef;

extern const ModuleDef module_defs[MT_COUNT];

typedef struct { u8 type, x, y, rot; s8 arc; u8 tier; } Placed;   /* rot: 0 up, 1 right, 2 down, 3 left;
                                                                    * tier 1-3 (0 counts as 1) */
#define TIER_MAX 3
static inline int tier_of(const Placed *p) { return p->tier ? p->tier : 1; }

/* Tiers (from blueprints): stronger and tougher, but heavier and dearer.
 *   value (thrust, power made, shields, energy, rounds, cargo, armor):  x1, x1.6, x2.4
 *   damage (weapons): x1, x1.4, x1.9     hit points: x1, x1.5, x2.2
 *   mass: x1, x1.4, x2 (armor: x1, x2, x3.3)     price: x1, x4, x12 */
int fit_tier_value(int value, int tier);
int fit_tier_mass(int type, int tier);
int fit_tier_hp(int type, int tier);
int fit_tier_price(int type, int tier);
int fit_tier_damage_pct(int tier);
int fit_blueprint_price(int type, int tier);   /* to unlock a tier of a module type */
int fit_needs_edge(int type);                  /* lasers and cannons: yes; turrets, racks, railguns: no */
int fit_engine_bay_ok(int type);               /* may go on rear engine squares besides engines */

typedef struct {
    u8 count;
    Placed m[LAYOUT_MAX];
} Layout;

typedef struct {
    u8 n;                     /* grid is n x n squares over the sprite */
    const u8 *zones;          /* n * n, Z_* */
    const Placed *fit;        /* the hull's default fit */
    u8 fit_count;
} HullGrid;

extern const HullGrid hull_grids[];   /* one per ship definition (hulls_data.c) */

/* A weapon on the ship, built from a layout. */
typedef struct {
    s16 x, y;                 /* position on the sprite: pixels right, pixels forward of the centre */
    s16 facing;               /* rest direction relative to the nose */
    s16 half_arc;
    s16 aim;                  /* current direction relative to the nose */
    s16 cool;
    u8  weapon, slew, type;
    u8  index;                /* which module of the layout it is */
    u8  tier;
} Mount;

/* What a layout adds up to (destroyed modules left out when hp is given). */
typedef struct {
    int power_out, power_use;     /* reactors; everything else */
    int thrust, mass;
    int shield, armor, energy, ammo, cargo, racks, weapons, fuel_cells;
    int has_bridge, life_support, cockpit;
    int warp_power, warp_biggest; /* sum of drive size x tier; largest drive size */
    int warp_size;                /* sum of drive sizes (S 1, M 2, L 3): sets fuel use */
} FitTotals;

void fit_totals_hp(const Placed *m, int count, const u8 *hp, FitTotals *t);
void fit_totals(const Layout *l, FitTotals *t);
void fit_default(int def_index, Layout *out);

/* Hull size class: 1 small (64 px sprite), 2 medium (128), 3 large (256). A hull
 * needs a warp drive of at least its class to jump. */
int  fit_size_class(int def_index);
/* Mass a hull may carry (before the Mass expansion upgrade): three per square,
 * or a quarter more than its default fit, whichever is bigger. */
int  fit_mass_allowance(int def_index);
/* Why a layout can't fly ("" if it can). mass_pct: allowance in percent (100 + upgrades). */
const char *fit_problem(int def_index, const Layout *l, int mass_pct);

/* Square (x, y) of a hull, or Z_OUT outside it. */
int  fit_zone(int def_index, int x, int y);
/* The module covering square (x, y), or -1. */
int  fit_at(const Layout *l, int x, int y);
/* Can module type go at (x, y) with rotation rot? Returns "" or the reason. */
const char *fit_check(int def_index, const Layout *l, int type, int x, int y, int rot);
void fit_size(int type, int rot, int *w, int *h);

/* Turns a layout into the ship's numbers. The hull keeps 70% of its catalog hull
 * points as its own structure; armor plating adds the rest (the default fit
 * comes out at the catalog figure). Shields come only from shield generators,
 * engines set acceleration and top speed against the total mass. The extra
 * results (energy, rounds, mounts) go in *x. */
struct ShipDef;
typedef struct {
    int energy_max, energy_regen;   /* regen: energy per second from spare power */
    int rounds_max, racks;
    int warp_ok;                    /* has a big enough warp drive */
    int warp_charge;                /* frames to charge a jump */
    int warp_fuel_pct;              /* fuel per jump, percent of normal (extra drives burn more) */
    int reactors, lifeline;         /* working reactors; working cockpit/bridge AND life support */
    int mount_count;
    Mount mounts[MOUNT_MAX];
} FitResult;
/* hp: each module's remaining hit points (0 = destroyed), or NULL for all intact. */
void fit_apply(int def_index, const Placed *m, int count, const u8 *hp, struct ShipDef *d, FitResult *x);

#endif
