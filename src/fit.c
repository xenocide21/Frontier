#include <stdio.h>
#include <string.h>

#include "fit.h"
#include "ship.h"
#include "weapon.h"
#include "sprite.h"

#define W WEAPON_NONE
const ModuleDef module_defs[MT_COUNT] = {
    /* name             tag   w  h  kind         rot weapon               half slew pow mass value price  hp boom  colour */
    { "",               "  ", 0, 0, MK_INTERNAL, 0, W,                      0,  0,   0,  0,   0,     0,  0,   0,  0, 0, 0 },
    { "COCKPIT",        "CK", 1, 1, MK_BRIDGE,   0, W,                      0,  0,  -1,  1,   0,     0, 40,   0, 255, 220, 80 },
    { "BRIDGE",         "BR", 1, 1, MK_BRIDGE,   0, W,                      0,  0,  -2,  4,   0,     0, 80,   0, 255, 220, 80 },
    { "LIFE SUPPORT",   "LF", 1, 1, MK_INTERNAL, 0, W,                      0,  0,  -1,  1,   0,   700, 30,   0, 120, 230, 120 },
    { "LASER",          "LS", 1, 1, MK_WEAPON,   1, WEAPON_LASER,         512, 40,   0,  2,   0,   600, 30,   0, 120, 255, 160 },
    { "CANNON",         "CN", 1, 2, MK_WEAPON,   1, WEAPON_CANNON,        341, 24,   0,  4,   0,   900, 40,   0, 255, 170, 90 },
    { "RAILGUN",        "RG", 2, 5, MK_WEAPON,   0, WEAPON_RAILGUN,       341,  8,   0, 50,   0,  6000,120,   0, 180, 200, 255 },
    { "LASER TURRET",   "LT", 1, 1, MK_WEAPON,   1, WEAPON_TURRET,       1536, 48,  -1,  2,   0,  1200, 30,   0, 120, 230, 200 },
    { "GUN TURRET",     "GT", 2, 2, MK_WEAPON,   1, WEAPON_GUN_TURRET,   1536, 30,  -1,  8,   0,  2400, 60,   0, 255, 150, 110 },
    { "HEAVY TURRET",   "HT", 3, 3, MK_WEAPON,   1, WEAPON_HEAVY_TURRET, 1536, 16,  -3, 18,   0,  7500,120,   0, 255, 120, 90 },
    { "MISSILE RACK",   "MR", 1, 2, MK_WEAPON,   1, WEAPON_ROCKET,          0,  0,   0,  2,   6,   800, 30,   0, 255, 200, 80 },
    { "ENGINE",         "EN", 1, 1, MK_ENGINE,   0, W,                      0,  0,  -2,  4,  60,   700, 40,   0, 90, 150, 255 },
    { "BIG ENGINE",     "BE", 2, 2, MK_ENGINE,   0, W,                      0,  0,  -7, 16, 280,  2600,100,   0, 70, 120, 255 },
    { "HUGE ENGINE",    "HE", 3, 3, MK_ENGINE,   0, W,                      0,  0, -15, 36, 700,  7800,200,   0, 50, 100, 255 },
    { "WARP DRIVE S",   "W1", 1, 1, MK_INTERNAL, 0, W,                      0,  0,  -1,  4,   1,  1500, 40,   0, 200, 120, 255 },
    { "WARP DRIVE M",   "W2", 2, 2, MK_INTERNAL, 0, W,                      0,  0,  -4, 16,   2,  6000, 90,   0, 180, 100, 255 },
    { "WARP DRIVE L",   "W3", 3, 3, MK_INTERNAL, 0, W,                      0,  0,  -8, 36,   3, 18000,180,   0, 160, 80, 255 },
    { "REACTOR",        "RE", 1, 1, MK_INTERNAL, 0, W,                      0,  0,  10,  4,   0,   900, 40,  40, 255, 120, 60 },
    { "BIG REACTOR",    "RR", 2, 2, MK_INTERNAL, 0, W,                      0,  0,  45, 16,   0,  3600,100, 100, 255, 90, 40 },
    { "HUGE REACTOR",   "HR", 3, 3, MK_INTERNAL, 0, W,                      0,  0, 110, 36,   0,  9800,200, 220, 255, 70, 30 },
    { "CAPACITOR",      "CP", 1, 1, MK_INTERNAL, 0, W,                      0,  0,   0,  1,  40,   500, 25,   0, 230, 230, 90 },
    { "SHIELD GEN",     "SG", 1, 1, MK_INTERNAL, 0, W,                      0,  0,  -3,  4,  20,   900, 35,   0, 110, 180, 255 },
    { "BIG SHIELD GEN", "SS", 2, 2, MK_INTERNAL, 0, W,                      0,  0, -12, 16,  90,  3400, 90,   0, 90, 160, 255 },
    { "ARMOR",          "AR", 1, 1, MK_INTERNAL, 0, W,                      0,  0,   0,  6,   1,   300, 80,   0, 160, 160, 170 },
    { "AMMO STORE",     "AM", 1, 1, MK_INTERNAL, 0, W,                      0,  0,   0,  1,  60,   300, 25,   0, 210, 170, 110 },
    { "CARGO BAY",      "CB", 1, 1, MK_INTERNAL, 0, W,                      0,  0,   0,  1,   4,   200, 25,   0, 190, 160, 120 },
    { "FUEL CELL",      "FC", 1, 1, MK_INTERNAL, 0, W,                      0,  0,   0,  2,   1,   400, 20,  70, 255, 160, 200 },
};
#undef W

int fit_tier_value(int value, int tier) { return tier >= 3 ? value * 12 / 5 : tier == 2 ? value * 8 / 5 : value; }
int fit_tier_damage_pct(int tier) { return tier >= 3 ? 190 : tier == 2 ? 140 : 100; }
int fit_tier_mass(int type, int tier) {
    int m = module_defs[type].mass;
    if (type == MT_ARMOR) return tier >= 3 ? m * 10 / 3 : tier == 2 ? m * 2 : m;
    return tier >= 3 ? m * 2 : tier == 2 ? m * 7 / 5 : m;
}
int fit_tier_hp(int type, int tier) {
    int h = module_defs[type].hp;
    h = tier >= 3 ? h * 11 / 5 : tier == 2 ? h * 3 / 2 : h;
    return h > 255 ? 255 : h;
}
int fit_tier_price(int type, int tier) {
    int p = module_defs[type].price;
    return tier >= 3 ? p * 12 : tier == 2 ? p * 4 : p;
}
int fit_blueprint_price(int type, int tier) {
    int p = module_defs[type].price;
    return tier >= 3 ? p * 60 : tier == 2 ? p * 15 : 0;
}
int fit_needs_edge(int type) { return type == MT_LASER || type == MT_CANNON; }
/* Besides engines, the rear engine squares take warp drives, reactors and armor. */
int fit_engine_bay_ok(int type) {
    return type == MT_WARP_S || type == MT_WARP_M || type == MT_WARP_L || type == MT_REACTOR ||
           type == MT_BIG_REACTOR || type == MT_HUGE_REACTOR || type == MT_ARMOR;
}

void fit_size(int type, int rot, int *w, int *h) {
    const ModuleDef *m = &module_defs[type];
    *w = (rot & 1) ? m->h : m->w;
    *h = (rot & 1) ? m->w : m->h;
}

int fit_zone(int def_index, int x, int y) {
    const HullGrid *g = &hull_grids[def_index];
    if (x < 0 || y < 0 || x >= g->n || y >= g->n) return Z_OUT;
    return g->zones[y * g->n + x];
}

int fit_at(const Layout *l, int x, int y) {
    for (int i = 0; i < l->count; i++) {
        const Placed *p = &l->m[i];
        int w, h;
        fit_size(p->type, p->rot, &w, &h);
        if (x >= p->x && x < p->x + w && y >= p->y && y < p->y + h) return i;
    }
    return -1;
}

const char *fit_check(int def_index, const Layout *l, int type, int x, int y, int rot) {
    const ModuleDef *m = &module_defs[type];
    int w, h, edge = 0;
    if (l->count >= LAYOUT_MAX) return "NO ROOM FOR MORE MODULES";
    if (rot && !m->rotates) return "THIS MODULE CANNOT TURN";
    fit_size(type, rot, &w, &h);
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            int z = fit_zone(def_index, x + i, y + j);
            if (z == Z_OUT) return "OUTSIDE THE HULL";
            if (fit_at(l, x + i, y + j) >= 0) return "SQUARE TAKEN";
            if (m->kind == MK_BRIDGE && z != Z_BRIDGE) return "BRIDGE SQUARE ONLY";
            if (m->kind == MK_ENGINE && z != Z_ENGINE) return "ENGINES GO IN THE REAR SQUARES";
            if (m->kind != MK_ENGINE && z == Z_ENGINE && !fit_engine_bay_ok(type))
                return "REAR SQUARES: ENGINES, WARP DRIVES, REACTORS, ARMOR";
            if (m->kind != MK_BRIDGE && z == Z_BRIDGE) return "THAT IS THE BRIDGE SQUARE";
            if (z == Z_EDGE) edge = 1;
        }
    if (fit_needs_edge(type) && !edge) return "LASERS AND CANNONS NEED AN EDGE SQUARE";
    return "";
}

void fit_totals_hp(const Placed *pl, int count, const u8 *hp, FitTotals *t) {
    memset(t, 0, sizeof *t);
    for (int i = 0; i < count; i++) {
        if (hp && hp[i] == 0) continue;                      /* destroyed */
        const ModuleDef *m = &module_defs[pl[i].type];
        int tier = tier_of(&pl[i]);
        int v = fit_tier_value(m->value, tier);
        if (m->power > 0) t->power_out += fit_tier_value(m->power, tier); else t->power_use -= m->power;
        t->mass += fit_tier_mass(pl[i].type, tier);
        switch (pl[i].type) {
        case MT_COCKPIT: t->has_bridge = 1; t->cockpit = 1; break;
        case MT_BRIDGE: t->has_bridge = 1; break;
        case MT_LIFE_SUPPORT: t->life_support++; break;
        case MT_ENGINE: case MT_BIG_ENGINE: case MT_HUGE_ENGINE: t->thrust += v; break;
        case MT_WARP_S: case MT_WARP_M: case MT_WARP_L:
            t->warp_power += m->value * tier;               /* better drives charge faster */
            t->warp_size += m->value;                       /* more drives burn more fuel */
            if (m->value > t->warp_biggest) t->warp_biggest = m->value;
            break;
        case MT_SHIELD_GEN: case MT_BIG_SHIELD_GEN: t->shield += v; break;
        case MT_ARMOR: t->armor += fit_tier_value(5, tier); break;   /* in fifths: 5, 8, 12 per plate */
        case MT_CAPACITOR: t->energy += v; break;
        case MT_AMMO_STORE: t->ammo += v; break;
        case MT_CARGO_BAY: t->cargo += v; break;
        case MT_FUEL_CELL: t->fuel_cells++; break;
        case MT_MISSILE_RACK: t->racks++; t->weapons++; break;
        default: if (m->kind == MK_WEAPON) t->weapons++; break;
        }
    }
}

void fit_totals(const Layout *l, FitTotals *t) { fit_totals_hp(l->m, l->count, 0, t); }

void fit_default(int def_index, Layout *out) {
    const HullGrid *g = &hull_grids[def_index];
    out->count = g->fit_count;
    memcpy(out->m, g->fit, g->fit_count * sizeof(Placed));
}

int fit_size_class(int def_index) {
    int size = sprite_size(ship_defs[def_index].sprite);
    return size >= 256 ? 3 : size >= 128 ? 2 : 1;
}

static int hull_squares(int def_index);

int fit_mass_allowance(int def_index) {
    const HullGrid *g = &hull_grids[def_index];
    FitTotals t0;
    fit_totals_hp(g->fit, g->fit_count, 0, &t0);
    int a = hull_squares(def_index) * 3, b = t0.mass * 125 / 100;
    return a > b ? a : b;
}

const char *fit_problem(int def_index, const Layout *l, int mass_pct) {
    static char why[48];
    FitTotals t;
    fit_totals(l, &t);
    int cls = fit_size_class(def_index);
    int allow = fit_mass_allowance(def_index) * mass_pct / 100;
    if (!t.has_bridge)                 return cls == 1 ? "NO COCKPIT" : "NO BRIDGE";
    if (cls > 1 && t.life_support == 0) return "NO LIFE SUPPORT";
    if (t.thrust == 0)                 return "NO ENGINES";
    if (t.power_out < t.power_use)     return "NOT ENOUGH POWER: ADD A REACTOR";
    if (t.mass > allow) {
        snprintf(why, sizeof why, "TOO HEAVY: MASS %d OF %d", t.mass, allow);
        return why;
    }
    if (t.warp_biggest < cls)
        return cls == 3 ? "NEEDS A WARP DRIVE L TO JUMP" : cls == 2 ? "NEEDS A WARP DRIVE M OR L TO JUMP" : "NEEDS A WARP DRIVE TO JUMP";
    return "";
}

/* ------------------------------------------------------------------------ */
/* Layout -> ship numbers and weapon mounts                                  */
/* ------------------------------------------------------------------------ */


static int hull_squares(int def_index) {
    const HullGrid *g = &hull_grids[def_index];
    int n = 0;
    for (int i = 0; i < g->n * g->n; i++) n += g->zones[i] != Z_OUT;
    return n;
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

void fit_apply(int def_index, const Placed *m, int count, const u8 *hp, ShipDef *d, FitResult *x) {
    const HullGrid *g = &hull_grids[def_index];
    FitTotals t, t0;
    fit_totals_hp(m, count, hp, &t);
    fit_totals_hp(g->fit, g->fit_count, 0, &t0);
    int base_mass = hull_squares(def_index);          /* the bare hull */
    int M = base_mass + t.mass, M0 = base_mass + t0.mass;

    /* Hull: 70% structure, armor plating adds the rest. */
    int structure = t0.armor ? d->hull_max * 70 / 100 : d->hull_max;   /* no default plating: all structure */
    /* armor counts in fifths of a tier 1 plate; the default plating gives exactly
     * the catalog hull (no rounding loss) */
    int plating = t0.armor ? (int) ((s32) (d->hull_max - structure) * t.armor / t0.armor)
                           : t.armor * (d->hull_max / 60 + 1);
    d->hull_max = (u16) clampi(structure + plating, 10, 65000);

    /* Shields: only from generators. */
    if (t0.shield > 0) {
        d->shield_arc_max  = (u16) clampi(d->shield_arc_max * t.shield / t0.shield, 0, 60000);
        d->shield_recharge = (u16) clampi(d->shield_recharge * t.shield / t0.shield, 0, 400);
    } else {
        d->shield_arc_max = (u16) (t.shield * 2);
        d->shield_recharge = (u16) (t.shield / 8);
    }

    /* Engines against mass: thrust-to-mass compared with the default fit.
     * Turning: heavier turns slower (the Maneuvering upgrade is applied later). */
    int r100 = (t0.thrust > 0 && M > 0) ? t.thrust * M0 * 100 / (M * t0.thrust) : 0;
    d->accel     = (u16) clampi(d->accel * r100 / 100, 0, 4000);
    d->max_speed = (u16) clampi(d->max_speed * clampi((100 + r100) / 2, 0, 140) / 100, 0, 60000);
    if (t.thrust == 0) d->max_speed = 0;
    d->turn_rate = (u16) clampi(d->turn_rate * clampi(M0 * 100 / (M ? M : 1), 60, 130) / 100, 1, 200);

    /* Fuel: half the tank is built into the hull, fuel cells hold the rest. */
    int base_fuel = d->fuel_cap / 2;
    int per_cell = t0.fuel_cells ? (d->fuel_cap - base_fuel) / t0.fuel_cells : 5;
    if (per_cell < 2) per_cell = 2;
    d->fuel_cap = (u8) clampi(base_fuel + t.fuel_cells * per_cell, 1, 250);

    d->cargo_cap = (u16) clampi(d->cargo_cap + t.cargo - t0.cargo, 0, 9999);
    d->weapon_mounts = (u8) t.weapons;

    x->energy_max   = 20 + t.energy;
    x->energy_regen = (t.power_out - t.power_use) * 2;
    if (x->energy_regen < 0) x->energy_regen = 0;
    x->rounds_max   = t.ammo;
    x->racks        = t.racks;
    x->reactors     = 0;
    for (int i = 0; i < count; i++)
        if ((!hp || hp[i]) && module_defs[m[i].type].power > 0) x->reactors++;
    /* (hp is per module, already scaled by tier when the ship was set up) */
    {
        int cls = fit_size_class(def_index);
        x->lifeline = t.has_bridge && (t.cockpit || t.life_support > 0 || cls == 1);
        x->warp_ok = t.warp_biggest >= cls;
        /* Extra drives charge faster but burn more fuel. */
        int wp = t.warp_power > 0 ? t.warp_power : 1;
        x->warp_charge = clampi(150 * cls / wp, 45, 150);
        x->warp_fuel_pct = clampi(100 * (t.warp_size ? t.warp_size : 1) / cls, 100, 400);
    }

    /* Weapon mounts: positions in sprite pixels (x right, y forward). */
    int size = sprite_size(d->sprite);
    if (size == 0) size = 64;
    int n = g->n;
    x->mount_count = 0;
    for (int i = 0; i < count && x->mount_count < MOUNT_MAX; i++) {
        const ModuleDef *md = &module_defs[m[i].type];
        if (md->kind != MK_WEAPON || (hp && hp[i] == 0)) continue;
        int w, h;
        fit_size(m[i].type, m[i].rot, &w, &h);
        Mount *mt = &x->mounts[x->mount_count++];
        mt->x = (s16) ((2 * m[i].x + w) * size / (2 * n) - size / 2);
        mt->y = (s16) (size / 2 - (2 * m[i].y + h) * size / (2 * n));
        mt->facing = (s16) (((m[i].rot * 1024) + m[i].arc * ARC_STEP) & ANGLE_MASK);
        mt->half_arc = md->half_arc;
        mt->aim = mt->facing;
        mt->cool = 0;
        mt->weapon = md->weapon;
        mt->slew = md->slew;
        mt->type = m[i].type;
        mt->index = (u8) i;
        mt->tier = (u8) tier_of(&m[i]);
    }
}
