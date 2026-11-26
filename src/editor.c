#include <stdio.h>
#include <string.h>
#include <psxgpu.h>
#include <psxgte.h>
#include <psxpad.h>

#include "editor.h"
#include "fit.h"
#include "gfx.h"
#include "input.h"
#include "outfit.h"
#include "sprite.h"

#define AREA_X   44          /* the grid area on the left */
#define AREA_Y   100
#define AREA_W   300
#define AREA_H   300
static int reset_confirm;     /* frames left to press Start again and reset the fit */
#define INFO_X   362         /* the info column on the right */
#define Z        0           /* text layer, in front of the dock panel */
#define ARC_LIMIT  8         /* arc offsets up to 8 steps (90 degrees) either way */

static int active, cx, cy, pick = MT_LASER, rot, arc, msg_timer, pick_tier = 1;
static Ship *es;               /* the ship being refitted (yours, or a hangar ship) */
static Layout *el;             /* its layout */
static char msg[64];
static int bx0, by0, bx1, by1;        /* the hull's bounding box on its grid */

static void say(const char *t) {
    strncpy(msg, t, sizeof msg - 1);
    msg[sizeof msg - 1] = 0;
    msg_timer = 180;
}

int editor_active(void) { return active; }

void editor_open(Ship *s, Layout *l) {
    es = s;
    el = l;
    const HullGrid *g = &hull_grids[s->def_index];
    bx0 = by0 = g->n; bx1 = by1 = -1;
    for (int y = 0; y < g->n; y++)
        for (int x = 0; x < g->n; x++)
            if (g->zones[y * g->n + x] != Z_OUT) {
                if (x < bx0) bx0 = x;
                if (x > bx1) bx1 = x;
                if (y < by0) by0 = y;
                if (y > by1) by1 = y;
            }
    cx = (bx0 + bx1) / 2;
    cy = (by0 + by1) / 2;
    rot = arc = 0;
    active = 1;
    msg_timer = 0;
}

/* Next placeable module type in direction d (the bridge is not for sale). */
static int next_type(int t, int d) {
    do { t = (t + d + MT_COUNT) % MT_COUNT; } while (t == MT_NONE || t == MT_BRIDGE || t == MT_COCKPIT);
    return t;
}

static void refit(Ship *s) {
    if (s->team == TEAM_PLAYER) outfit_refit(s);
    else ship_set_layout(s, el);          /* a hangar ship */
}

/* Start x2: every fitted module goes to storage, then the hull's default fit
 * goes on, from storage where you have the parts (fee) or bought new. */
static void reset_to_default(Ship *s, Layout *l) {
    static Layout d;
    char t[56];
    store_backup(0);
    for (int i = 0; i < l->count; i++) {
        int k = module_defs[l->m[i].type].kind;
        if (k != MK_BRIDGE) store_add(l->m[i].type, tier_of(&l->m[i]));
    }
    fit_default(s->def_index, &d);
    int cost = 0;
    for (int i = 0; i < d.count; i++) {
        const Placed *p = &d.m[i];
        if (module_defs[p->type].kind == MK_BRIDGE) continue;
        cost += store_take(p->type, tier_of(p)) ? fit_fee(p->type, tier_of(p)) : fit_tier_price(p->type, tier_of(p));
    }
    if (cost > credits) {
        store_backup(1);
        snprintf(t, sizeof t, "THE DEFAULT FIT COSTS %d: NOT ENOUGH CREDITS", cost);
        say(t);
        return;
    }
    credits -= cost;
    *l = d;
    refit(s);
    snprintf(t, sizeof t, "DEFAULT FIT ON: -%d. YOUR OLD MODULES ARE IN STORAGE", cost);
    say(t);
}

void editor_update(void) {
    Ship *s = es;
    Layout *l = el;
    if (msg_timer > 0) msg_timer--;

    if (input_pressed(PAD_LEFT)  && cx > bx0) cx--;
    if (input_pressed(PAD_RIGHT) && cx < bx1) cx++;
    if (input_pressed(PAD_UP)    && cy > by0) cy--;
    if (input_pressed(PAD_DOWN)  && cy < by1) cy++;
    if (input_pressed(PAD_SELECT)) pick_tier = pick_tier % TIER_MAX + 1;
    if (input_pressed(PAD_L1)) { pick = next_type(pick, -1); rot = 0; arc = 0; }
    if (input_pressed(PAD_R1)) { pick = next_type(pick, 1);  rot = 0; arc = 0; }
    if (input_pressed(PAD_SQUARE)) {
        if (module_defs[pick].rotates) rot = (rot + 1) & 3;
        else say("THIS MODULE CANNOT TURN");
    }

    int under = fit_at(l, cx, cy);
    if (input_pressed(PAD_L2) || input_pressed(PAD_R2)) {
        int d = input_pressed(PAD_L2) ? -1 : 1;
        if (under >= 0 && module_defs[l->m[under].type].kind == MK_WEAPON) {
            int a = l->m[under].arc + d;
            if (a < -ARC_LIMIT || a > ARC_LIMIT) say("THE ARC CANNOT SWING FURTHER");
            else { l->m[under].arc = (s8) a; refit(s); }
        } else if (module_defs[pick].kind == MK_WEAPON) {
            if (arc + d >= -ARC_LIMIT && arc + d <= ARC_LIMIT) arc += d;
        }
    }

    if (input_pressed(PAD_CROSS)) {
        const ModuleDef *m = &module_defs[pick];
        int price = fit_tier_price(pick, pick_tier);
        char t[56];
        if (blueprint[pick] < pick_tier) {
            /* Locked tier: Cross buys the blueprint (every tier up to this one). */
            int bp = 0;
            for (int k = blueprint[pick] + 1; k <= pick_tier; k++) bp += fit_blueprint_price(pick, k);
            if (credits < bp) {
                snprintf(t, sizeof t, "TIER COSTS %d: NOT ENOUGH CREDITS", bp);
                say(t);
            } else {
                credits -= bp;
                blueprint[pick] = (u8) pick_tier;
                snprintf(t, sizeof t, "TIER %d UNLOCKED: -%d CR", pick_tier, bp);
                say(t);
            }
        } else {
            const char *why = fit_check(s->def_index, l, pick, cx, cy, rot);
            int stored = store_count(pick, pick_tier) > 0;
            int cost = stored ? fit_fee(pick, pick_tier) : price;
            if (why[0]) say(why);
            else if (credits < cost) say("NOT ENOUGH CREDITS");
            else {
                if (stored) store_take(pick, pick_tier);
                Placed *p = &l->m[l->count++];
                p->type = (u8) pick; p->x = (u8) cx; p->y = (u8) cy; p->rot = (u8) rot;
                p->arc = (s8) (m->kind == MK_WEAPON ? arc : 0);
                p->tier = (u8) pick_tier;
                credits -= cost;
                refit(s);
                snprintf(t, sizeof t, stored ? "%s T%d FITTED FROM STORAGE: -%d" : "%s T%d BOUGHT AND FITTED: -%d",
                         m->name, pick_tier, cost);
                say(t);
            }
        }
    }
    if (input_pressed(PAD_CIRCLE)) {
        if (under < 0) say("NOTHING HERE. TRIANGLE WHEN DONE");
        else if (module_defs[l->m[under].type].kind == MK_BRIDGE) say("THE BRIDGE STAYS");
        else {
            store_add(l->m[under].type, tier_of(&l->m[under]));
            l->m[under] = l->m[--l->count];
            refit(s);
            say("MOVED TO STORAGE (SELL IT AT THE SHIPYARD IF YOU LIKE)");
        }
    }
    /* Start twice (within 3 seconds) resets to the default fit: one press could wipe a careful layout. */
    if (reset_confirm > 0) reset_confirm--;
    if (input_pressed(PAD_START)) {
        if (reset_confirm == 0) {
            reset_confirm = 180;
            say("PRESS START AGAIN: EVERYTHING TO STORAGE, DEFAULT FIT ON");
        } else {
            reset_confirm = 0;
            reset_to_default(s, l);
        }
    } else if (reset_confirm > 0 && input_pressed(PAD_CROSS | PAD_CIRCLE | PAD_TRIANGLE | PAD_SQUARE)) {
        reset_confirm = 0;
        say("RESET CANCELLED");
    }
    if (input_pressed(PAD_TRIANGLE)) {
        const char *p = fit_problem(s->def_index, l, outfit_mass_pct());
        if (p[0]) say(p);
        else active = 0;
    }
}

/* ------------------------------------------------------------------------ */

static void line(int x0, int y0, int x1, int y1, int r, int g, int b) {
    LINE_F2 *p = (LINE_F2 *) gfx_prim(Z, sizeof(LINE_F2));
    if (!p) return;
    setLineF2(p);
    setXY2(p, x0, y0, x1, y1);
    setRGB0(p, r, g, b);
}

static void outline(int x, int y, int w, int h, int r, int g, int b) {
    line(x, y, x + w, y, r, g, b);
    line(x, y + h, x + w, y + h, r, g, b);
    line(x, y, x, y + h, r, g, b);
    line(x + w, y, x + w, y + h, r, g, b);
}

/* Text in the editor never runs past the panel's right edge: long lines are cut. */
#define TEXT_RIGHT 612
static void text(int x, int y, int r, int g, int b, const char *t) {
    char cut[80];
    int max = (TEXT_RIGHT - x) / 8;
    if (max < 1) return;
    if ((int) strlen(t) > max && max < (int) sizeof cut) {
        memcpy(cut, t, (size_t) max);
        cut[max] = 0;
        t = cut;
    }
    gfx_text_big(x, y, Z, 1, r, g, b, t);
}

/* Two lines showing a weapon's arc, from the module's centre. */
static void arc_lines(int px, int py, int facing, int half, int len, int r, int g, int b) {
    for (int k = -1; k <= 1; k += 2) {
        int a = (facing + k * half) & ANGLE_MASK;
        line(px, py, px + ((isin(a) * len) >> FP_SHIFT), py - ((icos(a) * len) >> FP_SHIFT), r, g, b);
    }
    line(px, py, px + ((isin(facing) * (len / 2)) >> FP_SHIFT), py - ((icos(facing) * (len / 2)) >> FP_SHIFT), r, g, b);
}

void editor_draw(void) {
    const Ship *s = es;
    const Layout *l = el;
    const HullGrid *g = &hull_grids[s->def_index];
    char t[64];

    int bw = bx1 - bx0 + 1, bh = by1 - by0 + 1;
    int cp = AREA_W / (bw > bh ? bw : bh);
    if (cp > 28) cp = 28;
    int gx = AREA_X + (AREA_W - bw * cp) / 2, gy = AREA_Y + (AREA_H - bh * cp) / 2;
    #define SX(x) (gx + ((x) - bx0) * cp)
    #define SY(y) (gy + ((y) - by0) * cp)

    /* --- Info column --- */
    int y = AREA_Y + 14;                 /* below the dock's credits and clock */
    snprintf(t, sizeof t, "REFIT: %s%s", ship_looks[s->def_index].name, s->team == TEAM_PLAYER ? "" : " (HANGAR)");
    text(INFO_X, y - 14, 255, 230, 80, t);
    const ModuleDef *pm = &module_defs[pick];
    snprintf(t, sizeof t, "< %s >", pm->name);          /* the label goes in before its box: later additions draw underneath */
    gfx_button(INFO_X, y, 232, 22, Z, BTN_SELECTED, 1, t);                           y += 28;
    snprintf(t, sizeof t, "%dX%d  %s  POWER %+d  %d CR", pm->w, pm->h, pm->rotates ? "TURNS" : "FIXED",
             pm->power > 0 ? fit_tier_value(pm->power, pick_tier) : pm->power, fit_tier_price(pick, pick_tier));
    text(INFO_X, y, 230, 235, 245, t);                                              y += 12;
    if (blueprint[pick] >= pick_tier)
        snprintf(t, sizeof t, "TIER %d  MASS %d  HP %d", pick_tier, fit_tier_mass(pick, pick_tier),
                 fit_tier_hp(pick, pick_tier));
    else {
        int bp = 0;
        for (int k = blueprint[pick] + 1; k <= pick_tier; k++) bp += fit_blueprint_price(pick, k);
        snprintf(t, sizeof t, "TIER %d LOCKED  X: %d CR", pick_tier, bp);
    }
    text(INFO_X, y, blueprint[pick] >= pick_tier ? 255 : 255, blueprint[pick] >= pick_tier ? 230 : 140,
         blueprint[pick] >= pick_tier ? 80 : 90, t);                               y += 12;
    if (store_count(pick, pick_tier) > 0) {
        snprintf(t, sizeof t, "IN STORAGE %d  FIT FEE %d", store_count(pick, pick_tier), fit_fee(pick, pick_tier));
        text(INFO_X, y, 140, 220, 255, t);
    } else {
        text(INFO_X, y, 150, 170, 200, "NONE IN STORAGE: BUYS NEW");
    }
    y += 12;
    switch (pick) {
    case MT_ENGINE: case MT_BIG_ENGINE: case MT_HUGE_ENGINE:
        snprintf(t, sizeof t, "THRUST %d. REAR SQUARES ONLY", pm->value); break;
    case MT_REACTOR: case MT_BIG_REACTOR: case MT_HUGE_REACTOR:
        snprintf(t, sizeof t, "%d POWER. EXPLODES IF HIT", pm->power); break;
    case MT_WARP_S: case MT_WARP_M: case MT_WARP_L:
        snprintf(t, sizeof t, "MORE DRIVES: FASTER, MORE FUEL"); break;
    case MT_LIFE_SUPPORT: snprintf(t, sizeof t, "KEEPS THE CREW ALIVE. SPARES OK"); break;
    case MT_FUEL_CELL: snprintf(t, sizeof t, "JUMP FUEL. EXPLODES IF HIT"); break;
    case MT_CAPACITOR: snprintf(t, sizeof t, "STORES %d ENERGY FOR LASERS", pm->value); break;
    case MT_SHIELD_GEN: case MT_BIG_SHIELD_GEN: snprintf(t, sizeof t, "SHIELDS +%d STRENGTH", pm->value); break;
    case MT_ARMOR: snprintf(t, sizeof t, "HULL PLATING: MORE HULL POINTS"); break;
    case MT_AMMO_STORE: snprintf(t, sizeof t, "%d ROUNDS FOR CANNONS, GUNS", pm->value); break;
    case MT_CARGO_BAY: snprintf(t, sizeof t, "+%d CARGO", pm->value); break;
    case MT_MISSILE_RACK: snprintf(t, sizeof t, "LAUNCHES MISSILES (L2)"); break;
    default:
        snprintf(t, sizeof t, "ARC %d DEG  %s", pm->half_arc * 2 * 360 / 4096,
                 weapon_defs[pm->weapon].energy_cost ? "ENERGY" : "ROUNDS");
        break;
    }
    text(INFO_X, y, 150, 200, 230, t);                                              y += 12;
    if (pm->kind == MK_WEAPON) {
        text(INFO_X, y, 120, 220, 160, weapon_role_text(pm->weapon));               y += 12;
        snprintf(t, sizeof t, "PLACING: TURN %d  ARC %+d", rot * 90, arc * 45 / 4);
        text(INFO_X, y, 150, 170, 200, t);
    }
    y += 20;

    FitTotals ft;
    fit_totals(l, &ft);
    text(INFO_X, y, 255, 200, 90, "THIS FIT");                                       y += 14;
    snprintf(t, sizeof t, "POWER %d MADE, %d USED", ft.power_out, ft.power_use);
    text(INFO_X, y, ft.power_out >= ft.power_use ? 120 : 255, ft.power_out >= ft.power_use ? 220 : 110,
         ft.power_out >= ft.power_use ? 160 : 100, t);                               y += 12;
    snprintf(t, sizeof t, "ENERGY %d  +%d/S", s->energy_max, s->energy_regen);
    text(INFO_X, y, 230, 235, 245, t);                                              y += 12;
    snprintf(t, sizeof t, "SPEED %d PX/S  ACCEL %d", (int) ((s->def.max_speed * FRAMES_PER_SECOND) >> FP_SHIFT), s->def.accel);
    text(INFO_X, y, 230, 235, 245, t);                                              y += 12;
    {
        int allow = fit_mass_allowance(s->def_index) * outfit_mass_pct() / 100;
        snprintf(t, sizeof t, "MASS %d OF %d  TURN %d", ft.mass, allow, s->def.turn_rate);
        text(INFO_X, y, ft.mass <= allow ? 230 : 255, ft.mass <= allow ? 235 : 110, ft.mass <= allow ? 245 : 100, t);
        y += 12;
        snprintf(t, sizeof t, "FUEL %d  JUMP %s", s->def.fuel_cap,
                 !s->warp_ok ? "NO" : s->warp_charge < 150 ? "FAST" : "OK");
        text(INFO_X, y, 230, 235, 245, t);                                          y += 12;
    }
    snprintf(t, sizeof t, "HULL %d  SHIELD %d/ARC", s->def.hull_max, s->def.shield_arc_max);
    text(INFO_X, y, 230, 235, 245, t);                                              y += 12;
    snprintf(t, sizeof t, "GUNS %d  RACKS %d  ROUNDS %d", ft.weapons - ft.racks, ft.racks, s->rounds_max);
    text(INFO_X, y, 230, 235, 245, t);                                              y += 12;
    snprintf(t, sizeof t, "CARGO %d", s->def.cargo_cap);
    text(INFO_X, y, 230, 235, 245, t);                                              y += 18;
    const char *prob = fit_problem(s->def_index, l, outfit_mass_pct());
    if (prob[0]) text(INFO_X, y, 255, 110, 100, prob);
    else         text(INFO_X, y, 120, 220, 160, "READY TO FLY");
    y += 16;

    /* What is under the cursor. */
    int under = fit_at(l, cx, cy);
    if (under >= 0) {
        const Placed *p = &l->m[under];
        snprintf(t, sizeof t, "HERE: %s", module_defs[p->type].name);
        text(INFO_X, y, 140, 220, 255, t);                                           y += 12;
        if (module_defs[p->type].kind == MK_WEAPON) {
            snprintf(t, sizeof t, "ARC OFFSET %+d DEG (L2 R2)", p->arc * 45 / 4);
            text(INFO_X, y, 140, 220, 255, t);
        }
    } else {
        static const char *zn[5] = { "", "INSIDE SQUARE", "EDGE: HARDPOINT", "REAR: ENGINE WARP REACTOR ARMOR", "BRIDGE" };
        text(INFO_X, y, 150, 170, 200, zn[fit_zone(s->def_index, cx, cy)]);
    }

    if (msg_timer > 0) text(AREA_X, AREA_Y + AREA_H + 8, 255, 200, 90, msg);
    text(AREA_X, AREA_Y + AREA_H + 22, 150, 160, 180, "DPAD MOVE   L1 R1 MODULE   SELECT TIER   X FIT   O REMOVE");
    text(AREA_X, AREA_Y + AREA_H + 34, 150, 160, 180, "SQUARE TURN   L2 R2 ARC   START X2 DEFAULT FIT   TRIANGLE DONE");

    /* --- Grid: cursor first (on top), then arcs, modules, zone dots, lines, sprite --- */
    {
        int w, h;
        fit_size(pick, rot, &w, &h);
        int ok = fit_check(s->def_index, l, pick, cx, cy, rot)[0] == 0;
        int lum = (msg_timer & 8) ? 255 : 200;
        outline(SX(cx), SY(cy), w * cp, h * cp, ok ? 80 : lum, ok ? lum : 80, 80);
        outline(SX(cx) + 1, SY(cy) + 1, w * cp - 2, h * cp - 2, ok ? 80 : lum, ok ? lum : 80, 80);
        if (pm->kind == MK_WEAPON && pm->half_arc)
            arc_lines(SX(cx) + w * cp / 2, SY(cy) + h * cp / 2, ((rot * 1024) + arc * ARC_STEP) & ANGLE_MASK,
                      pm->half_arc, 40, 90, 255, 120);
    }
    for (int i = 0; i < l->count; i++) {
        const Placed *p = &l->m[i];
        const ModuleDef *m = &module_defs[p->type];
        int w, h;
        fit_size(p->type, p->rot, &w, &h);
        int px = SX(p->x), py = SY(p->y);
        if (m->kind == MK_WEAPON && m->half_arc && (i == under))
            arc_lines(px + w * cp / 2, py + h * cp / 2, ((p->rot * 1024) + p->arc * ARC_STEP) & ANGLE_MASK,
                      m->half_arc, 48, 255, 230, 80);
        else if (m->kind == MK_WEAPON) {
            int f = ((p->rot * 1024) + p->arc * ARC_STEP) & ANGLE_MASK;
            int mx = px + w * cp / 2, my = py + h * cp / 2;
            line(mx, my, mx + ((isin(f) * (cp - 2)) >> FP_SHIFT), my - ((icos(f) * (cp - 2)) >> FP_SHIFT), 255, 255, 255);
        }
        for (int k = 1; k < tier_of(p); k++)                       /* tier pips */
            gfx_rect(px + 2 + (k - 1) * 4, py + 2, 2, 2, Z, 255, 255, 255);
        if (cp >= 16 && w * cp >= 16) gfx_text_big(px + (w * cp - 16) / 2, py + (h * cp - 8) / 2, Z, 1, 20, 20, 30, m->tag);
        gfx_rect(px + 1, py + 1, w * cp - 1, h * cp - 1, Z, m->r * 3 / 4, m->g * 3 / 4, m->b * 3 / 4);
    }
    for (int yy = by0; yy <= by1; yy++)
        for (int xx = bx0; xx <= bx1; xx++) {
            int z = g->zones[yy * g->n + xx];
            if (z == Z_OUT) continue;
            static const u8 zc[5][3] = { {0,0,0}, {90,95,110}, {80,150,255}, {255,140,60}, {255,220,80} };
            int d = cp >= 10 ? 2 : 1;
            gfx_rect(SX(xx) + cp / 2 - d / 2, SY(yy) + cp / 2 - d / 2, d, d, Z, zc[z][0], zc[z][1], zc[z][2]);
        }
    for (int xx = bx0; xx <= bx1 + 1; xx++) line(SX(xx), SY(by0), SX(xx), SY(by1 + 1), 30, 40, 60);
    for (int yy = by0; yy <= by1 + 1; yy++) line(SX(bx0), SY(yy), SX(bx1 + 1), SY(yy), 30, 40, 60);

    /* The ship's sprite under the grid, dimmed, scaled so the grid squares match it. */
    {
        int size = sprite_size(ship_defs[s->def_index].sprite);
        if (size) {
            int scale = cp * g->n * 100 / size;
            int scx = gx + (g->n * cp) / 2 - bx0 * cp, scy = gy + (g->n * cp) / 2 - by0 * cp;
            sprite_draw(ship_defs[s->def_index].sprite, scx, scy, 0, scale, Z, 70, 70, 80);
        }
    }
    #undef SX
    #undef SY
}
