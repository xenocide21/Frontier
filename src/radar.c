#include <stdio.h>
#include <psxgpu.h>
#include <psxgte.h>

#include "enemy.h"
#include "escort.h"
#include "faction.h"
#include "galaxy.h"
#include "gfx.h"
#include "outfit.h"
#include "radar.h"
#include "weapon.h"
#include "world.h"

#define R_SIZE  128                         /* scope is R_SIZE x R_SIZE pixels */
#define R_HALF  (R_SIZE / 2)
#define R_X     (SCREEN_W - 24 - R_SIZE)    /* clear of the overscan edge */
#define R_Y     20

int radar_range(void) { return RADAR_BASE_RANGE + RADAR_STEP_RANGE * mod_level[MOD_RADAR]; }

/* Primitives in the same layer draw in reverse order of adding, so the scope
 * background (LAYER_BARS) is added last and everything else sits on top. */
static void dot(int x, int y, int size, int r, int g, int b) {
    gfx_rect(x - size / 2, y - size / 2, size, size, LAYER_HUD, r, g, b);
}

/* World offset (pixels) to scope position. Returns 1 if inside the scope.
 * With clamp set, outside points are pulled onto the rim instead. */
static int to_scope(int dx, int dy, int range, int clamp, int *sx, int *sy) {
    int lim = R_HALF - 3;
    int x = dx * R_HALF / range, y = dy * R_HALF / range;
    int inside = abs32(x) <= lim && abs32(y) <= lim;
    if (!inside) {
        if (!clamp) return 0;
        int m = abs32(x) > abs32(y) ? abs32(x) : abs32(y);
        x = x * lim / m;
        y = y * lim / m;
    }
    *sx = R_X + R_HALF + x;
    *sy = R_Y + R_HALF + y;
    return inside;
}

static void line_z(int z, int x0, int y0, int x1, int y1, int r, int g, int b) {
    LINE_F2 *l = (LINE_F2 *) gfx_prim(z, sizeof(LINE_F2));
    if (!l) return;
    setLineF2(l);
    setXY2(l, x0, y0, x1, y1);
    setRGB0(l, r, g, b);
}
#define line(...) line_z(LAYER_HUD, __VA_ARGS__)

static void ship_dot(const Ship *t, const Ship *me, int range, int r, int g, int b, int size) {
    int sx, sy;
    int dx = (int) FROM_FP(t->x - me->x), dy = (int) FROM_FP(t->y - me->y);
    if (to_scope(dx, dy, range, 0, &sx, &sy)) dot(sx, sy, size, r, g, b);
}

void radar_draw(const Ship *me, int target_slot, int frame) {
    int range = radar_range();
    int blink = (frame >> 4) & 1;
    int sx, sy;

    /* Station and planet: always shown, on the rim when out of range. */
    const NavObject *st = world_nav(NAV_STATION), *pl = world_nav(NAV_PLANET);
    int mx = (int) FROM_FP(me->x), my = (int) FROM_FP(me->y);
    const StarSystem *sys = galaxy_sys(galaxy_current());
    int in;
    if (!world_empty()) {                          /* empty space has neither (M16a) */
        in = to_scope(pl->x - mx, pl->y - my, range, 1, &sx, &sy);
        int psize = pl->radius * R_HALF / range * 2;
        if (psize < 5) psize = 5;
        if (!in) psize = 4;
        dot(sx, sy, psize, sys->planet_rgb[0], sys->planet_rgb[1], sys->planet_rgb[2]);
        in = to_scope(st->x - mx, st->y - my, range, 1, &sx, &sy);
        dot(sx, sy, 4, in ? 240 : 150, in ? 240 : 150, in ? 240 : 150);
    }

    /* NPC ships. */
    for (int k = 0; k < MAX_ENEMIES; k++) {
        const Ship *t = enemies_ship(k);
        if (!t) continue;
        int r = 255, g = 70, b = 50;                         /* pirate */
        int role = enemies_role(k);
        if (ship_disabled(t)) { r = g = b = 120; }
        else if (role == ROLE_MERCHANT) { r = 80; g = 230; b = 90; }
        else if (role == ROLE_PATROL) {
            int fac = t->team == TEAM_REBEL ? FAC_REBEL : FAC_FEDERATION;
            if (fac == FAC_REBEL) { r = 230; g = 80; b = 210; } else { r = 90; g = 160; b = 255; }
            if (faction_hostile(fac) && blink) { r = 255; g = 255; b = 255; }
        }
        ship_dot(t, me, range, r, g, b, 3);

        if (k == target_slot) {
            int dx = (int) FROM_FP(t->x - me->x), dy = (int) FROM_FP(t->y - me->y);
            if (to_scope(dx, dy, range, 0, &sx, &sy)) {
                line(sx - 4, sy - 4, sx - 1, sy - 4, 255, 230, 60);
                line(sx + 1, sy + 4, sx + 4, sy + 4, 255, 230, 60);
                line(sx - 4, sy - 4, sx - 4, sy - 1, 255, 230, 60);
                line(sx + 4, sy + 1, sx + 4, sy + 4, 255, 230, 60);
            }
        }
    }

    /* Your escorts. */
    for (int k = 0; k < MAX_ESCORTS; k++) {
        const Ship *e = escort_ship(k);
        if (e) ship_dot(e, me, range, 80, 230, 240, 3);
    }

    /* You: a small arrow at the centre, pointing along your heading. */
    {
        int cx = R_X + R_HALF, cy = R_Y + R_HALF;
        s32 sn = isin(me->angle), cs = icos(me->angle);
        POLY_F3 *p = (POLY_F3 *) gfx_prim(LAYER_HUD, sizeof(POLY_F3));
        if (p) {
            setPolyF3(p);
            setXY3(p,
                   cx + ((sn * 6) >> FP_SHIFT),                       cy - ((cs * 6) >> FP_SHIFT),
                   cx + ((-sn * 4 - cs * 4) >> FP_SHIFT),             cy - ((-cs * 4 + sn * 4) >> FP_SHIFT),
                   cx + ((-sn * 4 + cs * 4) >> FP_SHIFT),             cy - ((-cs * 4 - sn * 4) >> FP_SHIFT));
            setRGB0(p, 255, 255, 255);
        }
    }

    /* Frame, cross-hair and a half-range ring of ticks, then the background. */
    line(R_X, R_Y, R_X + R_SIZE, R_Y, 70, 130, 160);
    line(R_X, R_Y + R_SIZE, R_X + R_SIZE, R_Y + R_SIZE, 70, 130, 160);
    line(R_X, R_Y, R_X, R_Y + R_SIZE, 70, 130, 160);
    line(R_X + R_SIZE, R_Y, R_X + R_SIZE, R_Y + R_SIZE, 70, 130, 160);
    line_z(LAYER_BARS, R_X + R_HALF, R_Y + 2, R_X + R_HALF, R_Y + R_SIZE - 2, 20, 45, 60);
    line_z(LAYER_BARS, R_X + 2, R_Y + R_HALF, R_X + R_SIZE - 2, R_Y + R_HALF, 20, 45, 60);
    for (int i = 0; i < 16; i++) {
        int a = i * 256, x = R_X + R_HALF + ((isin(a) * (R_HALF / 2)) >> FP_SHIFT);
        int y = R_Y + R_HALF - ((icos(a) * (R_HALF / 2)) >> FP_SHIFT);
        gfx_rect(x, y, 1, 1, LAYER_BARS, 30, 70, 90);
    }
    gfx_rect(R_X, R_Y, R_SIZE, R_SIZE, LAYER_BARS, 6, 14, 22);

    char label[32];
    snprintf(label, sizeof label, "RADAR MK%d %d", mod_level[MOD_RADAR] + 1, range);
    gfx_text(R_X, R_Y + R_SIZE + 4, LAYER_HUD, label);
}
