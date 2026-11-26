#include <string.h>
#include <psxgpu.h>
#include <psxgte.h>
#include <psxpad.h>

#include "enemy.h"
#include "fx.h"
#include "gfx.h"
#include "input.h"
#include "rng.h"
#include "ship.h"
#include "sprite.h"
#include "outfit.h"
#include "weapon.h"
#include "audio.h"

/* The sandbox is limited so positions can never overflow 32 bits. */
#define WORLD_LIMIT   TO_FP(400000)
#define SHIELD_DELAY  180     /* frames after a hit before shields recharge */
#define RECHARGE_UNIT 6000    /* 60 frames x 100 (full generator) */
#define DISABLE_PCT   15      /* hull below this % disables the ship */

const char *ship_lost_module;

void ship_take_fit(Ship *s, const FitResult *fr) {
    s->warp_ok = (u8) fr->warp_ok;
    s->warp_charge = (u8) fr->warp_charge;
    s->warp_fuel_pct = (u16) fr->warp_fuel_pct;
    s->mount_count = (u8) fr->mount_count;
    memcpy(s->mounts, fr->mounts, fr->mount_count * sizeof(Mount));
    s->racks = (u8) fr->racks;
    s->energy_max = (s16) fr->energy_max;
    s->energy_regen = (s16) fr->energy_regen;
    s->rounds_max = (s16) fr->rounds_max;
    if (s->energy > s->energy_max) s->energy = s->energy_max;
    if (s->rounds > s->rounds_max) s->rounds = s->rounds_max;
}

void ship_reset_modules(Ship *s) {
    if (s->team == TEAM_PLAYER) {
        s->fit = player_layout.m;
        s->fit_count = player_layout.count;
    } else if (s->layout) {                       /* a fleet ship with its own fit */
        s->fit = s->layout->m;
        s->fit_count = s->layout->count;
    } else {
        const HullGrid *g = &hull_grids[s->def_index];
        s->fit = g->fit;
        s->fit_count = g->fit_count;
    }
    for (int i = 0; i < s->fit_count; i++) s->mod_hp[i] = (u8) fit_tier_hp(s->fit[i].type, tier_of(&s->fit[i]));
}

void ship_set_layout(Ship *s, Layout *l) {
    static FitResult fr;
    s->layout = l;
    ship_reset_modules(s);
    s->def = ship_defs[s->def_index];
    fit_apply(s->def_index, s->fit, s->fit_count, s->mod_hp, &s->def, &fr);
    s->def.cargo_cap = (u16) (s->def.cargo_cap * (100 + STORAGE_PCT * mod_level[MOD_STORAGE]) / 100);
    s->def.turn_rate = (u16) (s->def.turn_rate * (100 + MANEUVER_PCT * mod_level[MOD_MANEUVER]) / 100);
    ship_take_fit(s, &fr);
    if (s->hull > (int) s->def.hull_max) s->hull = s->def.hull_max;
}

int ship_modules_lost(const Ship *s) {
    int n = 0;
    for (int i = 0; i < s->fit_count; i++) n += s->mod_hp[i] == 0;
    return n;
}

static void ship_compute(Ship *s, FitResult *fr);
void ship_recompute(Ship *s) { static FitResult fr; ship_compute(s, &fr); }

int ship_modules_hurt(const Ship *s) {
    int n = 0;
    for (int i = 0; i < s->fit_count; i++) n += s->mod_hp[i] < module_defs[s->fit[i].type].hp;
    return n;
}

/* Recomputes the ship's numbers from its layout and what is left of each module. */
static void ship_compute(Ship *s, FitResult *fr) {
    if (s->team == TEAM_PLAYER) {
        outfit_compute(s->def_index, &s->def, &s->fire_pct, fr, s->mod_hp);   /* table entry, fit, upgrades */
        outfit_sync_fuel(s);       /* the tank follows the fuel cells (a lost cell loses its fuel) */
    } else {
        s->def = ship_defs[s->def_index];
        fit_apply(s->def_index, s->fit, s->fit_count, s->mod_hp, &s->def, fr);
        if (s->layout) {                            /* your fleet ships get your hull upgrades too */
            s->def.cargo_cap = (u16) (s->def.cargo_cap * (100 + STORAGE_PCT * mod_level[MOD_STORAGE]) / 100);
            s->def.turn_rate = (u16) (s->def.turn_rate * (100 + MANEUVER_PCT * mod_level[MOD_MANEUVER]) / 100);
        }
        s->fire_pct = 100;
    }
    ship_take_fit(s, fr);
}

static void ship_load_def(Ship *s) {
    static FitResult fr;
    if (s->team == TEAM_PLAYER) outfit_compute(s->def_index, &s->def, &s->fire_pct, &fr, 0);   /* sets up the layout */
    ship_reset_modules(s);
    ship_compute(s, &fr);
}

static void ship_reset_stats(Ship *s) {
    const ShipDef *d = &s->def;
    s->hull = d->hull_max;
    for (int i = 0; i < ARC_COUNT; i++) { s->shield[i] = (s16) d->shield_arc_max; s->shield_flash[i] = 0; }
    for (int i = 0; i < SYS_COUNT; i++) s->sys[i] = 100;
    s->shield_delay = 0;
    s->shield_acc = 0;
    s->disabled_time = 0;
    for (int t = 0; t < MSL_COUNT; t++) s->msl[t] = (s16) ship_missile_max(s, t);
    s->ammo = s->msl[MSL_ROCKET];
    if (s->msl_sel >= MSL_COUNT || ship_missile_max(s, s->msl_sel) == 0) s->msl_sel = MSL_ROCKET;
    s->cool[0] = s->cool[1] = s->cool[2] = 0;
    s->turret_aim = 0;
    s->energy = s->energy_max;
    s->energy_acc = 0;
    s->rounds = s->rounds_max;
    s->flash = 0;
}

void ship_init(Ship *s, int def_index, int team) {
    s->layout = 0;
    s->x = s->y = 0;
    s->vx = s->vy = 0;
    s->angle = 0;
    s->assist = 1;
    s->boost = 0;
    s->thrust = 0;
    s->def_index = def_index;
    s->team = team;
    ship_load_def(s);
    s->loadout = (team == TEAM_PLAYER) ? &ship_loadouts[def_index]
               : (team == TEAM_ENEMY || team == TEAM_FED || team == TEAM_REBEL || team == TEAM_ALLY) ? &enemy_loadout : &unarmed_loadout;
    s->respawn = 0;
    s->invuln = 0;
    s->docked = 0;
    ship_reset_stats(s);
}

void ship_set_def(Ship *s, int def_index) {
    /* Position, velocity and heading carry over; everything else refits full. */
    s->def_index = def_index;
    ship_load_def(s);
    s->loadout = &ship_loadouts[def_index];
    ship_reset_stats(s);
}

s32 ship_speed(const Ship *s) {
    return approx_len(s->vx, s->vy);
}

void ship_controls_from_pad(ShipControls *c) {
    /* D-pad or left stick steers and thrusts; right stick strafes. */
    c->turn = 0;
    if (input_held(PAD_LEFT)  || input_stick(0, 0) < -40) c->turn -= 1;
    if (input_held(PAD_RIGHT) || input_stick(0, 0) >  40) c->turn += 1;

    c->fwd = 0;
    if (input_held(PAD_UP)   || input_stick(0, 1) < -40) c->fwd += FP_ONE;        /* full forward thrust */
    if (input_held(PAD_DOWN) || input_stick(0, 1) >  40) c->fwd -= FP_ONE / 2;    /* reverse is half as strong */

    c->side = (s16) (input_stick(1, 0) * (FP_ONE / 2) / 128);   /* right stick: strafe, + = right */

    c->cruise = 0;
    c->boost = (u8) input_held(PAD_CROSS);       /* Cross: afterburner */
    c->fire1 = (u8) input_held(PAD_R2);          /* R2: all lasers and guns */
    c->fire2 = (u8) input_held(PAD_L2);          /* L2: the selected missile */
    c->target1 = 0;                              /* the caller fills in the combat target */
}

int ship_missile_max(const Ship *s, int type) {
    const ShipLoadout *l = s->loadout;
    if (l->secondary == WEAPON_NONE || s->racks == 0) return 0;
    return type == MSL_ROCKET ? l->ammo_max : type == MSL_SEEKER ? l->seeker_max : l->ion_max;
}

int ship_next_missile(Ship *s) {
    for (int k = 1; k <= MSL_COUNT; k++) {
        int t = (s->msl_sel + k) % MSL_COUNT;
        if (ship_missile_max(s, t) > 0) { s->msl_sel = (u8) t; break; }
    }
    return s->msl_sel;
}

/* ------------------------------------------------------------------------ */
/* Damage                                                                    */
/* ------------------------------------------------------------------------ */

/* Which shield arc faces a hit at world pixel (hx, hy)? */
static int hit_arc(const Ship *s, int hx, int hy) {
    int dx = hx - (int) FROM_FP(s->x);
    int dy = hy - (int) FROM_FP(s->y);
    s32 sn = isin(s->angle);
    s32 cs = icos(s->angle);
    int f = (dx * sn - dy * cs) >> FP_SHIFT;   /* distance ahead of the nose direction */
    int r = (dx * cs + dy * sn) >> FP_SHIFT;   /* distance to the right */
    int af = f < 0 ? -f : f;
    int ar = r < 0 ? -r : r;
    if (af >= ar) return f >= 0 ? ARC_FRONT : ARC_BACK;
    return r >= 0 ? ARC_RIGHT : ARC_LEFT;
}

/* Ion hit: no hull damage. Every shield arc loses the damage, and the engines
 * and weapons lose half of it each, shields or not. Zero engines = disabled. */
static int ion_damage(Ship *s, int damage, int hx, int hy) {
    int was_disabled = ship_disabled(s);
    int arc = hit_arc(s, hx, hy);
    s->shield_delay = SHIELD_DELAY;
    for (int i = 0; i < ARC_COUNT; i++) {
        s->shield[i] -= (s16) damage;
        if (s->shield[i] < 0) s->shield[i] = 0;
    }
    s->shield_flash[arc] = 10;
    s->sys[SYS_ENGINE]  -= (s16) (damage / 2);
    s->sys[SYS_WEAPONS] -= (s16) (damage / 2);
    if (s->sys[SYS_ENGINE] < 0)  s->sys[SYS_ENGINE] = 0;
    if (s->sys[SYS_WEAPONS] < 0) s->sys[SYS_WEAPONS] = 0;
    fx_burst(s->x, s->y, 8, 7000, 18, FX_SPARK);
    audio_sfx_world((!was_disabled && ship_disabled(s)) ? SFX_POWERDOWN : SFX_HIT_SHIELD, (int) FROM_FP(s->x), (int) FROM_FP(s->y));
    return (!was_disabled && ship_disabled(s)) ? DMG_DISABLED : DMG_NONE;
}

/* ------------------------------------------------------------------------ */
/* Location damage                                                           */
/* ------------------------------------------------------------------------ */

/* Centre of module i on the sprite, in pixels: x right, y forward. */
static void module_centre(const Ship *s, int i, int *x, int *y) {
    const HullGrid *g = &hull_grids[s->def_index];
    int size = sprite_size(ship_defs[s->def_index].sprite);
    if (!size) size = 64;
    int w, h;
    fit_size(s->fit[i].type, s->fit[i].rot, &w, &h);
    *x = (2 * s->fit[i].x + w) * size / (2 * g->n) - size / 2;
    *y = size / 2 - (2 * s->fit[i].y + h) * size / (2 * g->n);
}

enum { MD_NONE = 0, MD_LOST = 1, MD_BREACH = 2, MD_DISABLED = 4 };
static int blast_hull;          /* hull damage from explosions during one hit */

static void damage_module(Ship *s, int i, int dmg, int depth);

/* A destroyed reactor or fuel cell explodes: the modules within two squares of
 * it take the blast (which can set off more), and so does the hull. */
static void explode_module(Ship *s, int i, int depth) {
    int boom = module_defs[s->fit[i].type].boom;
    if (!boom) return;
    blast_hull += boom / 2;
    int cx, cy;
    module_centre(s, i, &cx, &cy);
    s32 sn = isin(s->angle), cs = icos(s->angle);
    fx_burst(s->x + cx * cs + cy * sn, s->y + cx * sn - cy * cs, 12, 9000, 22, FX_FIRE);
    audio_sfx_world(SFX_EXPLODE_S, (int) FROM_FP(s->x), (int) FROM_FP(s->y));
    int w, h;
    fit_size(s->fit[i].type, s->fit[i].rot, &w, &h);
    int x0 = s->fit[i].x - 2, y0 = s->fit[i].y - 2, x1 = s->fit[i].x + w + 1, y1 = s->fit[i].y + h + 1;
    for (int j = 0; j < s->fit_count; j++) {
        if (j == i || s->mod_hp[j] == 0) continue;
        int wj, hj;
        fit_size(s->fit[j].type, s->fit[j].rot, &wj, &hj);
        if (s->fit[j].x > x1 || s->fit[j].x + wj - 1 < x0 || s->fit[j].y > y1 || s->fit[j].y + hj - 1 < y0) continue;
        damage_module(s, j, boom, depth + 1);
    }
}

static void damage_module(Ship *s, int i, int dmg, int depth) {
    if (s->mod_hp[i] == 0) return;
    if (dmg >= s->mod_hp[i]) {
        s->mod_hp[i] = 0;
        if (s->team == TEAM_PLAYER) ship_lost_module = module_defs[s->fit[i].type].name;
        if (depth < 4) explode_module(s, i, depth);
    } else {
        s->mod_hp[i] -= (u8) dmg;
    }
}

/* Damage that got through the shields lands on the module nearest the impact.
 * Returns MD_* flags. */
static int hit_modules(Ship *s, int hx, int hy, int dmg) {
    if (s->fit_count == 0) return MD_NONE;
    int dx = hx - (int) FROM_FP(s->x), dy = hy - (int) FROM_FP(s->y);
    s32 sn = isin(s->angle), cs = icos(s->angle);
    int lx = (dx * cs + dy * sn) >> FP_SHIFT;          /* right of the centre line */
    int ly = (dx * sn - dy * cs) >> FP_SHIFT;          /* ahead of the centre */
    int best = -1, best_d = 0x7fffffff;
    for (int i = 0; i < s->fit_count; i++) {
        if (s->mod_hp[i] == 0) continue;
        int mx, my;
        module_centre(s, i, &mx, &my);
        int d = (mx - lx) * (mx - lx) + (my - ly) * (my - ly);
        if (d < best_d) { best_d = d; best = i; }
    }
    if (best < 0) return MD_NONE;
    int lost_before = ship_modules_lost(s);
    blast_hull = 0;
    damage_module(s, best, dmg, 0);
    if (ship_modules_lost(s) == lost_before) return MD_NONE;

    /* Something broke: the ship's numbers change. */
    static FitResult fr;
    int hull = s->hull;
    ship_compute(s, &fr);
    s->hull = hull - blast_hull;
    if (s->hull > (int) s->def.hull_max) s->hull = s->def.hull_max;
    for (int a = 0; a < ARC_COUNT; a++)
        if (s->shield[a] > (s16) s->def.shield_arc_max) s->shield[a] = (s16) s->def.shield_arc_max;
    int flags = MD_LOST;
    if (fr.reactors == 0) flags |= MD_BREACH;           /* no power left: the core goes */
    if (!fr.lifeline) {                                   /* no bridge or no life support */
        s->sys[SYS_ENGINE] = 0;
        flags |= MD_DISABLED;
    }
    return flags;
}

/* Capital shields turn aside this much of every hit, by class (M13f). */
static int shield_hardness(int cls) {
    switch (cls) {
    case CLASS_CORVETTE:  return 2;
    case CLASS_FRIGATE:   return 4;
    case CLASS_DESTROYER: return 6;
    case CLASS_CRUISER:   return 8;
    default:              return 0;
    }
}

static int d_big(const Ship *s) { return s->def.hull_max >= 600; }   /* corvettes and up go up big (M19a) */

int ship_damage(Ship *s, int damage, int hx, int hy) {
    if (s->respawn > 0 || s->invuln || s->docked) return 0;
    if (proj_hit_weapon == WEAPON_ION) return ion_damage(s, damage, hx, hy);

    /* 1. The facing shield arc takes the hit first. Each weapon has its own
     * strength against shields and against hull; a capital ship's shield
     * also deflects a fixed part of every hit. */
    int sp = 100, hp = 100, hard = 0;
    if (proj_hit_weapon != WEAPON_NONE && proj_hit_weapon < WEAPON_COUNT) {
        sp = weapon_roles[proj_hit_weapon].shield_pct;
        hp = weapon_roles[proj_hit_weapon].hull_pct;
        hard = shield_hardness(s->def.cls);
    }
    int arc = hit_arc(s, hx, hy);
    if (s->shield[arc] > 0) {
        s->shield_delay = SHIELD_DELAY;
        s->shield_flash[arc] = 10;
        int sd = damage * sp / 100 - hard;        /* the hit, as the shield feels it */
        if (sd < 1) sd = 1;
        if (s->shield[arc] >= sd) {
            s->shield[arc] -= (s16) sd;
            audio_sfx_world(SFX_HIT_SHIELD, (int) FROM_FP(s->x), (int) FROM_FP(s->y));
            return 0;
        }
        /* The shield breaks: what it did not stop goes on to the hull. */
        damage = damage * (sd - s->shield[arc]) / sd;
        s->shield[arc] = 0;
    } else {
        s->shield_delay = SHIELD_DELAY;
    }
    damage = damage * hp / 100;
    if (damage <= 0) return 0;

    /* 2. The rest hits the hull and breaks part of one subsystem. */
    int was_disabled = ship_disabled(s);
    s->hull -= damage;
    s->flash = 3;
    audio_sfx_world(SFX_HIT_HULL, (int) FROM_FP(s->x), (int) FROM_FP(s->y));
    int k = rng_range(SYS_COUNT);
    s->sys[k] -= (s16) (damage * 2);
    if (s->sys[k] < 0) s->sys[k] = 0;
    int md = hit_modules(s, hx, hy, damage);           /* and the module where it hit */
    if (md & MD_BREACH) {
        if (s->team == TEAM_PLAYER) ship_lost_module = "ALL REACTORS LOST: CORE BREACH";
        s->hull = 0;
    }

    if (s->hull > 0) {
        /* Low hull: engines and reactor fail. The ship is disabled, not dead. */
        if (s->hull * 100 < (int) s->def.hull_max * DISABLE_PCT) {
            s->sys[SYS_ENGINE] = 0;
            s->sys[SYS_REACTOR] = 0;
        }
        if (!was_disabled && ship_disabled(s)) {
            audio_sfx_world(SFX_POWERDOWN, (int) FROM_FP(s->x), (int) FROM_FP(s->y));
            return DMG_DISABLED;
        }
        return DMG_NONE;
    }

    /* 3. Destroyed. */
    s->hull = 0;
    s->respawn = 90;
    s->thrust = s->boost = 0;
    fx_burst(s->x, s->y, 24, 12000, 30, FX_FIRE);
    fx_burst(s->x, s->y, 10, 16000, 20, FX_SPARK);
    audio_sfx_world(s->team == TEAM_PLAYER || d_big(s) ? SFX_EXPLODE_L : SFX_EXPLODE_S, (int) FROM_FP(s->x), (int) FROM_FP(s->y));
    return DMG_DESTROYED;
}

int ship_repair(Ship *s) {
    const ShipDef *d = &s->def;
    static int tick;
    int changed = 0;

    if (ship_modules_hurt(s)) {                       /* modules are rebuilt first, all at once */
        static FitResult fr;
        for (int i = 0; i < s->fit_count; i++) s->mod_hp[i] = (u8) fit_tier_hp(s->fit[i].type, tier_of(&s->fit[i]));
        ship_compute(s, &fr);
        changed = 1;
    }
    if (s->hull < d->hull_max) { s->hull++; changed = 1; }
    for (int i = 0; i < ARC_COUNT; i++)
        if (s->shield[i] < (s16) d->shield_arc_max) { s->shield[i]++; changed = 1; }
    for (int i = 0; i < SYS_COUNT; i++)
        if (s->sys[i] < 100) { s->sys[i]++; changed = 1; }
    if (s->rounds < s->rounds_max) { s->rounds += 4; if (s->rounds > s->rounds_max) s->rounds = s->rounds_max; changed = 1; }
    if (s->energy < s->energy_max) { s->energy = s->energy_max; changed = 1; }
    if ((++tick & 15) == 0)
        for (int t = 0; t < MSL_COUNT; t++)
            if (s->msl[t] < ship_missile_max(s, t)) { s->msl[t]++; changed = 1; }
    s->ammo = s->msl[MSL_ROCKET];
    return changed;
}

/* ------------------------------------------------------------------------ */
/* Flight                                                                    */
/* ------------------------------------------------------------------------ */

void ship_update(Ship *s, const ShipControls *c) {
    const ShipDef *d = &s->def;

    if (s->flash) s->flash--;
    for (int i = 0; i < ARC_COUNT; i++)
        if (s->shield_flash[i]) s->shield_flash[i]--;
    if (s->cool[0] > 0) s->cool[0]--;
    if (s->cool[1] > 0) s->cool[1]--;

    /* Destroyed (player only; AI ships are not updated while destroyed):
     * wait, then come back at the start with everything repaired. */
    if (s->respawn > 0) {
        if (--s->respawn == 0) {
            s->x = s->y = 0;
            s->vx = s->vy = 0;
            s->angle = 0;
            ship_reset_stats(s);
        }
        return;
    }

    /* --- Disabled: sparks fly. A disabled player is towed home after 10 s. --- */
    if (ship_disabled(s)) {
        if ((rng_next() & 15) == 0)
            fx_spawn(s->x, s->y, rng_range(1025) - 512, rng_range(1025) - 512, 12, FX_SPARK);
        if (s->team == TEAM_PLAYER && ++s->disabled_time >= 600) {
            s->respawn = 90;
            s->disabled_time = 0;
        }
    } else {
        s->disabled_time = 0;
    }

    /* --- Shields: recharge after a quiet spell, at the generator's rate --- */
    if (s->shield_delay > 0) {
        s->shield_delay--;
    } else {
        s->shield_acc += (s16) ((d->shield_recharge * s->sys[SYS_SHIELDGEN] * s->sys[SYS_REACTOR]) / 100);
        while (s->shield_acc >= RECHARGE_UNIT) {
            s->shield_acc -= RECHARGE_UNIT;
            for (int i = 0; i < ARC_COUNT; i++)
                if (s->shield[i] < (s16) d->shield_arc_max) s->shield[i]++;
        }
    }

    /* --- Engine damage: weaker thrust and top speed, slower turning --- */
    int eng = s->sys[SYS_ENGINE];
    int power_pct = eng <= 0 ? 0 : 20 + (80 * eng) / 100;   /* 0, or 20..100 % */
    int turn_pct  = eng <= 0 ? 0 : 50 + eng / 2;            /* 0, or 50..100 % */
    if (eng < 60 && (rng_next() & 7) == 0)    /* smoking engines */
        fx_spawn(s->x, s->y, 0, 0, 16, FX_TRAIL);

    /* --- Rotation --- */
    s->angle += c->turn * ((d->turn_rate * turn_pct) / 100);
    s->angle &= ANGLE_MASK;

    /* --- Thrust amounts --- */
    s32 acc = (d->accel * power_pct) / 100;
    s32 max = (d->max_speed * power_pct) / 100;
    s->boost = (c->boost && power_pct > 0) || c->cruise;
    if (s->boost) {
        acc *= 2;          /* afterburner: double thrust ... */
        max += max >> 1;   /* ... and 50% higher top speed */
    }

    s32 fwd = c->fwd, side = c->side;
    if (fwd < 0 && !d->brakes) fwd = 0;     /* no retro-thrusters: no reverse thrust */
    s->thrust = (fwd > 0 && power_pct > 0) || c->cruise;

    /* Heading vector (forward) and the ship's right-hand vector, FP12.
     * Angle 0 points up the screen (negative y); angle 1024 points right. */
    s32 sn = isin(s->angle);
    s32 cs = icos(s->angle);
    s32 ax = (sn * fwd + cs * side) >> FP_SHIFT;
    s32 ay = (-cs * fwd + sn * side) >> FP_SHIFT;

    if (!c->cruise) {
        s->vx += (ax * acc) >> FP_SHIFT;
        s->vy += (ay * acc) >> FP_SHIFT;
    }

    /* --- Slowing down off the throttle ---
     * Ships with retro-thrusters (heavy ships) brake on their own; others drift
     * and must turn round and burn, unless flight assist is on. A ship with dead
     * engines drifts to a gentle stop either way. */
    int coasting = !c->cruise && fwd == 0 && side == 0;
    int shift = 0;
    if (!c->cruise && power_pct == 0)  shift = 5;
    else if (coasting && d->brakes)    shift = d->brakes;
    else if (coasting && s->assist)    shift = 5;
    if (shift) {
        s->vx -= s->vx >> shift;
        s->vy -= s->vy >> shift;
        if (abs32(s->vx) < 8) s->vx = 0;
        if (abs32(s->vy) < 8) s->vy = 0;
    }

    /* --- Top speed: scale the velocity back if it is too long --- */
    s32 len = approx_len(s->vx, s->vy);
    if (!c->cruise && power_pct > 0 && len > max) {
        s32 ratio = (max << FP_SHIFT) / len;   /* FP12, below 1.0 */
        s->vx = (s->vx * ratio) >> FP_SHIFT;
        s->vy = (s->vy * ratio) >> FP_SHIFT;
    }

    /* --- Move, and stop at the edge of the sandbox --- */
    s->x += s->vx;
    s->y += s->vy;
    if (s->x >  WORLD_LIMIT) { s->x =  WORLD_LIMIT; s->vx = 0; }
    if (s->x < -WORLD_LIMIT) { s->x = -WORLD_LIMIT; s->vx = 0; }
    if (s->y >  WORLD_LIMIT) { s->y =  WORLD_LIMIT; s->vy = 0; }
    if (s->y < -WORLD_LIMIT) { s->y = -WORLD_LIMIT; s->vy = 0; }
}

/* Heading (0 = up, 1024 = right) of the direction dx, dy, to about a degree. */
static int heading_of(s32 dx, s32 dy) {
    s32 ax = abs32(dx), ay = abs32(dy), sum = ax + ay;
    if (sum == 0) return 0;
    int r = (int) (1024 * ax / sum);
    if (dy < 0) return dx >= 0 ? r : (4096 - r) & ANGLE_MASK;
    return dx >= 0 ? 2048 - r : 2048 + r;
}

static inline int angle_diff(int a, int b) { return ((a - b + 2048) & ANGLE_MASK) - 2048; }

#define AIM_TOLERANCE 96     /* a gun fires once its aim is this close (about 8 degrees) */

/* Every weapon mount: track the target inside its arc (or swing back to its
 * facing), and fire when R2 / the AI asks and the gun can bear. Missiles (L2)
 * leave from the missile racks. */
void ship_fire(Ship *s, const ShipControls *c) {
    if (s->respawn > 0) return;

    /* Capacitors recharge from spare reactor power. */
    if (s->sys[SYS_REACTOR] > 0 && s->energy < s->energy_max) {
        s->energy_acc += s->energy_regen;
        while (s->energy_acc >= FRAMES_PER_SECOND) { s->energy_acc -= FRAMES_PER_SECOND; s->energy++; }
        if (s->energy > s->energy_max) s->energy = s->energy_max;
    }

    s32 sn = isin(s->angle);
    s32 cs = icos(s->angle);
    int slow = 200 - s->sys[SYS_WEAPONS];          /* damaged weapons fire more slowly */
    int armed = s->sys[SYS_REACTOR] > 0 && s->sys[SYS_WEAPONS] > 0;
    const Ship *tgt = c->tgt;
    if (tgt && !ship_alive(tgt)) tgt = 0;
    int tslot = (int) c->target1 - 1;
    int rack_k = 0;

    for (int i = 0; i < s->mount_count; i++) {
        Mount *m = &s->mounts[i];
        if (m->cool > 0) m->cool--;
        /* Mount position in the world (FP12): x right, y forward on the sprite. */
        s32 mx = s->x + m->x * cs + m->y * sn;
        s32 my = s->y + m->x * sn - m->y * cs;

        if (m->type == MT_MISSILE_RACK) {
            /* Missiles: one launch per press of L2, taking the racks in turn. */
            if (c->fire2 && s->cool[1] <= 0 && rack_k++ == (s->ammo & 7) % (s->racks ? s->racks : 1)) {
                int type = s->msl_sel;
                int wid = missile_weapon[type];
                const WeaponDef *w = &weapon_defs[wid];
                if (s->msl[type] >= w->ammo_per_shot) {
                    int dir = (s->angle + m->facing) & ANGLE_MASK;
                    s32 vx = s->vx + ((isin(dir) * (s32) w->proj_speed) >> FP_SHIFT);
                    s32 vy = s->vy - ((icos(dir) * (s32) w->proj_speed) >> FP_SHIFT);
                    int home = (type == MSL_SEEKER && tgt && tslot >= 0) ? tslot : 255;
                    if (proj_spawn_homing(mx, my, vx, vy, wid, s->team, home)) {
                        s->msl[type] -= w->ammo_per_shot;
                        s->ammo = s->msl[MSL_ROCKET];
                        s->cool[1] = (s16) ((w->fire_interval * slow) / 100);
                    }
                }
            }
            continue;
        }

        int wid = m->weapon;
        if (s->team == TEAM_ENEMY && wid == WEAPON_LASER) wid = WEAPON_PIRATE_LASER;   /* pirates: weak guns */
        const WeaponDef *w = &weapon_defs[wid];

        /* Where should this gun point? */
        int want = m->facing, bears = 0;
        if (tgt) {
            s32 dx = FROM_FP(tgt->x - mx), dy = FROM_FP(tgt->y - my);
            s32 d = approx_len(dx, dy);
            if (d > 0 && d <= w->range) {
                s32 frames = (d << FP_SHIFT) / w->proj_speed;
                s32 px = dx + FROM_FP((tgt->vx - s->vx) * frames);
                s32 py = dy + FROM_FP((tgt->vy - s->vy) * frames);
                int rel = (heading_of(px, py) - s->angle) & ANGLE_MASK;
                if (abs32(angle_diff(rel, m->facing)) <= m->half_arc) { want = rel; bears = 1; }
            }
        }
        int turn = angle_diff(want, m->aim);
        if (turn > m->slew) turn = m->slew; else if (turn < -m->slew) turn = -m->slew;
        m->aim = (s16) ((m->aim + turn) & ANGLE_MASK);

        if (!c->fire1 || m->cool > 0 || !armed) continue;
        if (tgt ? !bears : 0) continue;                                    /* can't see the target */
        if (abs32(angle_diff(want, m->aim)) > AIM_TOLERANCE) continue;     /* still swinging round */
        if (w->energy_cost > s->energy) continue;
        if (w->ammo_per_shot > s->rounds) continue;
        int dir = (s->angle + m->aim) & ANGLE_MASK;
        s32 vx = s->vx + ((isin(dir) * (s32) w->proj_speed) >> FP_SHIFT);
        s32 vy = s->vy - ((icos(dir) * (s32) w->proj_speed) >> FP_SHIFT);
        proj_spawn_tier = m->tier;
        int fired = proj_spawn(mx, my, vx, vy, wid, s->team);
        proj_spawn_tier = 1;
        if (fired) {
            s->energy -= w->energy_cost;
            s->rounds -= w->ammo_per_shot;
            m->cool = (s16) ((w->fire_interval * slow / 100) * s->fire_pct / 100);
        }
    }
}

/* ------------------------------------------------------------------------ */
/* Drawing                                                                   */
/* ------------------------------------------------------------------------ */

/* Converts a point in ship space (x right, y forward, pixels) to a screen
 * offset, given the sine and cosine of the heading. */
static inline void to_screen(int px, int py, s32 sn, s32 cs, int *ox, int *oy) {
    *ox = (px * cs + py * sn) >> FP_SHIFT;
    *oy = (px * sn - py * cs) >> FP_SHIFT;
}

/* Draws the glow of a recently hit shield arc: four short lines on a circle
 * a few pixels outside the hull, covering a quarter turn. */
static void draw_shield_arc(const Ship *s, int arc, int cx, int cy, int R) {
    /* A struck shield lights up: a bright edge with the glow falling away inside it. */
    int fl = s->shield_flash[arc];
    int rad = R + 5;
    int base = (int) s->angle + arc * 1024 - 512;
    int px = 0, py = 0, qx = 0, qy = 0;
    u32 edge = (u32) (fl * 10) << 16 | (u32) (fl * 20 > 255 ? 255 : fl * 20) << 8 | 255u * (u32) (fl > 4 ? 10 : fl * 2) / 10;
    for (int seg = 0; seg <= 6; seg++) {
        int a = (base + seg * 1024 / 6) & ANGLE_MASK;
        int x = cx + ((isin(a) * rad) >> FP_SHIFT), y = cy - ((icos(a) * rad) >> FP_SHIFT);
        int ix = cx + ((isin(a) * (rad - 9)) >> FP_SHIFT), iy = cy - ((icos(a) * (rad - 9)) >> FP_SHIFT);
        if (seg > 0) {
            gfx_line(px, py, x, y, LAYER_SHIP_TOP, 0xFFFFFF, 0xFFFFFF, 1);
            gfx_tri_g(px, py, x, y, qx, qy, LAYER_SHIP_TOP, edge, edge, 0, 1);
            gfx_tri_g(x, y, ix, iy, qx, qy, LAYER_SHIP_TOP, edge, 0, 0, 1);
        }
        px = x;  py = y;  qx = ix;  qy = iy;
    }
}

void ship_draw(const Ship *s, s32 cam_x, s32 cam_y, int frame) {
    const ShipLook *look = &ship_looks[s->def_index];
    int R  = look->radius;
    int cx = SCREEN_W / 2 + FROM_FP(s->x - cam_x);
    int cy = SCREEN_H / 2 + FROM_FP(s->y - cam_y);

    if (s->respawn > 0) return;   /* destroyed */

    /* Skip ships that are completely off screen. */
    if (cx < -40 || cx > SCREEN_W + 40 || cy < -40 || cy > SCREEN_H + 40)
        return;

    s32 sn = isin(s->angle);
    s32 cs = icos(s->angle);
    int x0, y0, x1, y1, x2, y2;

    /* Engine flame, behind the sprite. It flickers every other frame. */
    int T = look->tail;
    if (s->thrust) {
        /* A cone of light fading out behind the engines, with a hot glow at the
         * nozzle; longer and whiter on the afterburner. Bigger ships, bigger plume. */
        int big = R / 14 + 1;
        int len = ((s->boost ? 20 : 9) + ((frame & 2) ? 3 : 0)) * (big + 1) / 2;
        int w = 3 + big;
        to_screen(-w, -T + 4, sn, cs, &x0, &y0);
        to_screen( w, -T + 4, sn, cs, &x1, &y1);
        to_screen(0, -T - len, sn, cs, &x2, &y2);
        u32 hot = s->boost ? 0xFFF0C8 : 0xA0D2FF, cool = s->boost ? 0x3C1E00 : 0x00143C;
        gfx_tri_g(cx + x0, cy + y0, cx + x1, cy + y1, cx + x2, cy + y2, LAYER_SHIP_UNDER, hot, hot, cool, 1);
        to_screen(0, -T + 1, sn, cs, &x0, &y0);
        int gr = 7 + big * 3 + (s->boost ? 5 : 0) + (frame & 1);
        if (s->boost) gfx_glow(cx + x0, cy + y0, gr, gr, LAYER_SHIP_UNDER, 255, 170, 80);
        else          gfx_glow(cx + x0, cy + y0, gr, gr, LAYER_SHIP_UNDER, 70, 140, 255);
    }

    /* Hull: the ship's sprite, tinted by side (128 = as rendered). */
    {
        int r = 128, g = 128, b = 128;
        if (s->flash)                     { r = g = b = 255; }
        else if (s->team == TEAM_FED)     { r = 105; g = 125; b = 165; }
        else if (s->team == TEAM_REBEL)   { r = 150; g = 105; b = 150; }
        else if (s->team == TEAM_ALLY)    { r = 110; g = 145; b = 140; }
        int spr = ship_defs[s->def_index].sprite;
        if (spr < SPRITE_COUNT) {
            sprite_draw(spr, cx, cy, s->angle, 100, LAYER_SHIP, r, g, b);
        } else {
            /* No sprite yet: the old dart shape. */
            POLY_F3 *hull = (POLY_F3 *) gfx_prim(LAYER_SHIP, sizeof(POLY_F3));
            if (hull) {
                to_screen(0,             R,              sn, cs, &x0, &y0);
                to_screen(-(R * 7) / 10, -(R * 8) / 10,  sn, cs, &x1, &y1);
                to_screen( (R * 7) / 10, -(R * 8) / 10,  sn, cs, &x2, &y2);
                setPolyF3(hull);
                setXY3(hull, cx + x0, cy + y0, cx + x1, cy + y1, cx + x2, cy + y2);
                if (s->flash)                      setRGB0(hull, 255, 255, 255);
                else if (s->team == TEAM_ENEMY)    setRGB0(hull, 255, 150, 30);
                else if (s->team == TEAM_MERCHANT) setRGB0(hull, 110, 230, 120);
                else if (s->team == TEAM_ALLY)     setRGB0(hull, 70, 230, 210);
                else if (s->team == TEAM_FED)      setRGB0(hull, 90, 170, 255);
                else if (s->team == TEAM_REBEL)    setRGB0(hull, 230, 70, 200);
                else                               setRGB0(hull, look->r, look->g, look->b);
            }
            POLY_F3 *ck = (POLY_F3 *) gfx_prim(LAYER_SHIP_TOP, sizeof(POLY_F3));
            if (ck) {
                to_screen(0,             (R * 5) / 10,  sn, cs, &x0, &y0);
                to_screen(-(R * 2) / 10, -(R * 1) / 10, sn, cs, &x1, &y1);
                to_screen( (R * 2) / 10, -(R * 1) / 10, sn, cs, &x2, &y2);
                setPolyF3(ck);
                setXY3(ck, cx + x0, cy + y0, cx + x1, cy + y1, cx + x2, cy + y2);
                setRGB0(ck, 230, 240, 255);
            }
        }
    }

    /* Guns: a short barrel at every weapon mount, pointing where it aims. */
    {
        int blen = 3 + sprite_size(ship_defs[s->def_index].sprite) / 40;
        for (int i = 0; i < s->mount_count; i++) {
            const Mount *m = &s->mounts[i];
            int gx = cx + (int) ((m->x * cs + m->y * sn) >> FP_SHIFT);
            int gy = cy + (int) ((m->x * sn - m->y * cs) >> FP_SHIFT);
            int dir = (s->angle + m->aim) & ANGLE_MASK;
            LINE_F2 *bl = (LINE_F2 *) gfx_prim(LAYER_SHIP_TOP, sizeof(LINE_F2));
            if (!bl) break;
            setLineF2(bl);
            setXY2(bl, gx, gy, gx + ((isin(dir) * blen) >> FP_SHIFT), gy - ((icos(dir) * blen) >> FP_SHIFT));
            setRGB0(bl, 200, 205, 215);
        }
    }

    for (int a = 0; a < ARC_COUNT; a++)
        if (s->shield_flash[a]) draw_shield_arc(s, a, cx, cy, R);
}
