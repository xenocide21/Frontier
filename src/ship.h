/*
 * FRONTIER - ships: definitions (data) and the flight, firing and damage model.
 *
 * Player and AI ships share one Ship struct and one update function. What
 * differs is who fills in the ShipControls each frame: the pad, or the AI.
 *
 * Damage model (Milestone 3):
 *   1. Four shield arcs (front, right, back, left). A hit is taken by the arc
 *      facing it. Shields recharge 3 seconds after the last hit.
 *   2. What gets through lowers the hull and damages one random subsystem.
 *   3. Subsystems: engines (speed, acceleration, turning), reactor (at 0:
 *      weapons and shield recharge stop), shield generator (recharge rate) and
 *      weapons (fire rate; at 0 the primary guns are dead).
 *   4. Hull below 15%: the ship is DISABLED. Engines and reactor are gone, it
 *      drifts and cannot fire, but it still exists. Keep shooting and the hull
 *      reaches 0: destroyed. Stopping in time is the player's choice.
 */
#ifndef FRONTIER_SHIP_H
#define FRONTIER_SHIP_H

#include "common.h"
#include "weapon.h"
#include "fit.h"

enum {
    CLASS_SCOUT = 0, CLASS_LIGHT_FIGHTER, CLASS_HEAVY_FIGHTER, CLASS_CORVETTE,
    CLASS_FRIGATE, CLASS_DESTROYER, CLASS_CRUISER, CLASS_TRANSPORT,
    CLASS_FREIGHTER, CLASS_MINING_VESSEL, CLASS_SALVAGE_VESSEL
};

enum { ARC_FRONT = 0, ARC_RIGHT, ARC_BACK, ARC_LEFT, ARC_COUNT };
enum { SYS_ENGINE = 0, SYS_REACTOR, SYS_SHIELDGEN, SYS_WEAPONS, SYS_COUNT };
enum { DMG_NONE = 0, DMG_DISABLED, DMG_DESTROYED };   /* result of ship_damage */

/* Speeds and accelerations are FP12 pixels per frame (per frame^2). */
typedef struct ShipDef {
    u8  id, cls, maker, sprite;
    u16 hull_max;
    u16 shield_arc_max;      /* points in each of the four arcs */
    u16 shield_recharge;     /* points per second per arc */
    u16 engine_power;        /* not used yet */
    u16 max_speed;
    u16 accel;
    u16 turn_rate;           /* angle units per frame, 4096 = one full turn */
    u16 reactor_out;         /* not used yet */
    u16 cargo_cap;           /* not used yet */
    u8  crew_cap, fuel_cap;  /* not used yet */
    u8  weapon_mounts, util_mounts;
    u16 mass_allow;          /* not used yet */
    u32 price;               /* not used yet */
    u8  brakes;              /* retro-thrusters: 0 = none (the ship drifts until you turn
                              * round and burn; no reverse thrust), else the ship sheds
                              * 1/2^brakes of its speed each frame off the throttle */
} ShipDef;

/* Temporary look, before ships have textures. */
typedef struct {
    s16         radius;      /* pixels */
    u8          r, g, b;
    const char *name;        /* upper case: the debug font is upper case */
    s16         tail;        /* pixels from the centre to the engines on the sprite */
} ShipLook;

/* What a ship carries until the grid ship editor exists. */
typedef struct {
    u8 primary;              /* weapon id fired by every weapon mount */
    u8 secondary;            /* WEAPON_ROCKET if the ship has a missile rack, else WEAPON_NONE */
    u8 ammo_max;             /* rockets */
    u8 seeker_max, ion_max;  /* seeker and ion missiles */
    u8 turrets;              /* point-defence turrets: fire on hostiles by themselves */
} ShipLoadout;

/* Ship definitions, in the same order as the sprites (tools/shipgen). */
enum {
    DEF_WAYFARER = 0,
    DEF_RAIDER = 1,
    DEF_HAULER = 2,
    DEF_FRONTIER_RUNNER = 3,
    DEF_WAYFINDER = 4,
    DEF_ROCKBREAKER = 5,
    DEF_RECLAIMER = 6,
    DEF_WATCHMAN = 7,
    DEF_TALON = 8,
    DEF_VANGUARD = 9,
    DEF_SENTINEL = 10,
    DEF_BASTION = 11,
    DEF_RESOLUTE = 12,
    DEF_SOVEREIGN = 13,
    DEF_GHOST = 14,
    DEF_WASP = 15,
    DEF_HAMMER = 16,
    DEF_RENEGADE = 17,
    DEF_DEFIANT = 18,
    DEF_REPRISAL = 19,
    DEF_LIBERTY = 20,
    DEF_SKITTER = 21,
    DEF_MANTIS = 22,
    DEF_REAVER = 23,
    DEF_BROODGUARD = 24,
    DEF_LEVIATHAN = 25,
    DEF_SCRAPPER = 26,           /* Outer Rim Syndicate scrap-builds (M13g) */
    DEF_JACKAL = 27,
    DEF_CHOPSHOP = 28,
    DEF_MARAUDER = 29,
    SHIP_DEF_COUNT
};
extern const ShipDef     ship_defs[SHIP_DEF_COUNT];
extern const ShipLook    ship_looks[SHIP_DEF_COUNT];
extern const ShipLoadout ship_loadouts[SHIP_DEF_COUNT];
extern const ShipLoadout enemy_loadout;
extern const ShipLoadout unarmed_loadout;
extern const u8 ship_buyable[SHIP_DEF_COUNT];

struct Ship;

/* What the pilot (pad or AI) asks the ship to do this frame. */
typedef struct {
    s8  turn;                /* -1 left, 0 straight, +1 right */
    u8  boost, fire1, fire2;
    u8  cruise;              /* 1 = the autopilot sets velocity directly: no thrust, drag or speed cap */
    s16 fwd;                 /* FP12 fraction of thrust: +4096 full ahead, -2048 half astern */
    s16 side;                /* FP12 fraction of strafe thrust, + = right */
    u8  target1;             /* combat target for gun arcs and seekers: enemy slot + 1, 0 = none
                              * (so zeroed controls mean no target) */
    const struct Ship *tgt;  /* the ship the weapon mounts track and fire at, or NULL:
                              * then every gun fires straight along its own facing */
} ShipControls;

typedef struct Ship {
    s32 x, y;            /* FP12 world position, pixels */
    s32 vx, vy;          /* FP12 pixels per frame */
    s32 angle;           /* 0..4095, 0 = up */
    int def_index;
    ShipDef def;         /* this ship's stats: the table entry plus any fitted modules */
    u8  fire_pct;        /* weapon cooldown in percent of normal (100 = stock) */
    int assist;          /* 1 = flight assist: gentle drag off the throttle even without
                          * retro-thrusters (an easy-mode aid; NPC pilots always use it) */
    int boost;           /* 1 while the afterburner is on */
    int thrust;          /* 1 while forward thrust is on */
    int team;            /* TEAM_PLAYER, TEAM_ENEMY or TEAM_MERCHANT (see weapon.h) */
    const ShipLoadout *loadout;

    s32 hull;
    s16 shield[ARC_COUNT];        /* current points per arc */
    u8  shield_flash[ARC_COUNT];  /* frames of shield glow left */
    s16 shield_delay;             /* frames until recharging may start */
    s16 shield_acc;               /* recharge accumulator */
    s16 sys[SYS_COUNT];           /* subsystem health, 0..100 */
    s16 ammo;                     /* rockets */
    s16 msl[MSL_COUNT];           /* missiles of each type (msl[MSL_ROCKET] mirrors ammo) */
    u8  msl_sel;                  /* selected missile type, MSL_* */
    s16 cool[3];                  /* frames until guns / missiles / turrets may fire again */
    s16 turret_aim;               /* (unused since the fitting system) */
    /* From the fitted modules (fit.c): */
    Mount mounts[MOUNT_MAX];      /* every weapon, with its own facing, arc and aim */
    u8  mount_count, racks;
    s16 energy, energy_max;       /* capacitor charge for energy weapons */
    s16 energy_regen, energy_acc; /* per second, from spare reactor power */
    s16 rounds, rounds_max;       /* kinetic rounds in the ammo stores */
    /* Location damage (M13b): the fitted modules and what is left of each. */
    const Placed *fit;            /* the player's layout, a fleet layout, or the hull's default fit */
    Layout *layout;               /* fleet ships: their own fit (NULL = the hull's default) */
    u8  fit_count;
    u8  mod_hp[LAYOUT_MAX];       /* 0 = destroyed */
    u8  warp_ok;                  /* has a big enough working warp drive */
    u8  warp_charge;              /* frames to charge a jump */
    u16 warp_fuel_pct;            /* fuel per jump, percent of normal */
    s16 respawn;                  /* > 0 while destroyed (player: frames until return) */
    s16 disabled_time;            /* frames the player has been disabled */
    u8  flash;                    /* frames of hull-hit flash left */
    u8  invuln;                   /* 1 = takes no damage (used by stress mode) */
    u8  docked;                   /* 1 = docked at a station or planet: safe, not drawn */
} Ship;

void ship_init(Ship *s, int def_index, int team);
void ship_set_def(Ship *s, int def_index);       /* player: change ship, refit */
void ship_recompute(Ship *s);                    /* after mod_hp changed (loading a save) */

void ship_controls_from_pad(ShipControls *c);

/* Advances the ship by one frame. */
void ship_update(Ship *s, const ShipControls *c);

/* Fires weapons if the controls ask for it and the cooldowns allow it. */
void ship_fire(Ship *s, const ShipControls *c);

/* Applies damage from a hit at world pixel (hit_x, hit_y). Returns DMG_DESTROYED
 * if this hit destroyed the ship, DMG_DISABLED if it just disabled it. */
int ship_damage(Ship *s, int damage, int hit_x, int hit_y);

/* Missiles the ship's rack holds of a type (0 if it has no rack). */
int ship_missile_max(const Ship *s, int type);
/* Copies a fit's mounts, energy and rounds onto the ship (after its def changed). */
void ship_take_fit(Ship *s, const FitResult *fr);
/* Points the ship at its layout again and makes every module whole (after the
 * layout changed in the editor, or a full repair). */
void ship_reset_modules(Ship *s);
/* Gives a (non-player) ship its own layout and recomputes it, all modules whole. */
void ship_set_layout(Ship *s, Layout *l);
/* Modules destroyed / damaged (for the status screen and repair). */
int  ship_modules_lost(const Ship *s);
int  ship_modules_hurt(const Ship *s);
/* Name of the player's module destroyed most recently (main shows it), or NULL. */
extern const char *ship_lost_module;
/* Selects the next missile type the ship carries. Returns the new type. */
int ship_next_missile(Ship *s);



/* Restores a little of everything. Returns 1 if anything needed repair. */
int ship_repair(Ship *s);

/* Draws the ship relative to the camera position (FP12 world pixels). */
void ship_draw(const Ship *s, s32 cam_x, s32 cam_y, int frame);

/* Current speed in FP12 pixels per frame. */
s32 ship_speed(const Ship *s);

/* Disabled: engines gone, the ship cannot move or manoeuvre. */
static inline int ship_disabled(const Ship *s) { return s->sys[SYS_ENGINE] <= 0; }

/* True while the ship exists (not destroyed). */
static inline int ship_alive(const Ship *s) { return s->respawn <= 0; }

#endif
