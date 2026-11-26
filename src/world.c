#include <psxgpu.h>
#include <psxgte.h>

#include <stdio.h>

#include "galaxy.h"
#include "gfx.h"
#include "model.h"
#include "world.h"

#define LAYER_PLANET 9
#define PLANET_MAX_RADIUS 180   /* large, but always smaller than the whole screen */

/* ------------------------------------------------------------------------ */
/* The current system's fixed objects (built from the galaxy table)          */
/* ------------------------------------------------------------------------ */

static NavObject nav_objects[NAV_MAX];
static int nav_count;
static int cur_system;
static int planet_rgb[3] = { 50, 110, 170 };
static int planet_ready;

void world_set_system(int id) {
    const StarSystem *s = galaxy_sys(id);

    if (!s->station_name) {                    /* empty space (M16a): nothing to fly to or hit */
        nav_objects[NAV_STATION] = (NavObject){ "NOTHING", 30000, 30000, 0, 0, KIND_STATION };
        nav_objects[NAV_PLANET]  = (NavObject){ "NOTHING", 30000, -30000, 0, 0, KIND_PLANET };
        nav_count = 0;
    } else {
        nav_objects[NAV_STATION] = (NavObject){ s->station_name, 0, 0, 46, 50, KIND_STATION };
        nav_objects[NAV_PLANET]  = (NavObject){ s->planet_name, s->planet_x, s->planet_y,
                                                s->planet_radius, 40, KIND_PLANET };
        nav_count = NAV_MAX;
    }
    cur_system = id;
    for (int i = 0; i < 3; i++) planet_rgb[i] = s->planet_rgb[i];
    planet_ready = 0;
}

const NavObject *world_nav(int index) { return &nav_objects[index]; }
int world_nav_count(void) { return nav_count; }

int world_empty(void) { return nav_count == 0; }

int world_jump_lock(s32 x, s32 y, int *need) {
    if (nav_count == 0) { if (need) *need = 0; return -1; }
    int px = (int) FROM_FP(x), py = (int) FROM_FP(y);
    const NavObject *pl = &nav_objects[NAV_PLANET], *st = &nav_objects[NAV_STATION];
    int dp = (int) approx_len(px - pl->x, py - pl->y) - pl->radius - JUMP_LOCK_PLANET;
    int ds = (int) approx_len(px - st->x, py - st->y) - JUMP_LOCK_STATION;
    int worst = dp < ds ? NAV_PLANET : NAV_STATION;
    int gap = dp < ds ? dp : ds;
    if (need) *need = gap < 0 ? -gap : 0;
    return gap < 0 ? worst : -1;
}

/* The old jump-ring positions are kept in the galaxy table as arrival points:
 * a ship coming from a linked system appears there. */
void world_arrival_point(int from, int *x, int *y) {
    const StarSystem *s = galaxy_sys(cur_system);
    int j = galaxy_link_index(cur_system, from);
    if (j < 0) j = 0;
    *x = s->jump_x[j];
    *y = s->jump_y[j];
}

/* ------------------------------------------------------------------------ */
/* Stars                                                                     */
/* ------------------------------------------------------------------------ */

/*
 * One layer of stars.
 *   parallax_shift  how much slower than the camera the layer moves (1 = half)
 *   cell_shift      stars are placed in square cells of (1 << cell_shift) pixels
 *   skip16          chance out of 16 that a cell has no star
 *   bright_base     minimum brightness; bright_mask must be 2^n - 1
 */
static void star_layer(s32 cam_x, s32 cam_y, int layer, int z,
                       int parallax_shift, int cell_shift, int skip16,
                       int bright_base, int bright_mask) {
    s32 lx = FROM_FP(cam_x) >> parallax_shift;
    s32 ly = FROM_FP(cam_y) >> parallax_shift;
    s32 size = 1 << cell_shift;

    s32 cx0 = (lx - SCREEN_W / 2) >> cell_shift;
    s32 cx1 = (lx + SCREEN_W / 2) >> cell_shift;
    s32 cy0 = (ly - SCREEN_H / 2) >> cell_shift;
    s32 cy1 = (ly + SCREEN_H / 2) >> cell_shift;

    for (s32 cy = cy0; cy <= cy1; cy++) {
        for (s32 cx = cx0; cx <= cx1; cx++) {
            u32 h = hash3(cx, cy, layer);
            if ((int)(h >> 28) < skip16)
                continue;

            s32 px = cx * size + (s32)(h & (u32)(size - 1));
            s32 py = cy * size + (s32)((h >> 8) & (u32)(size - 1));
            s32 sx = px - lx + SCREEN_W / 2;
            s32 sy = py - ly + SCREEN_H / 2;
            if (sx < 0 || sx >= SCREEN_W || sy < 0 || sy >= SCREEN_H)
                continue;

            /* Two pixels tall: a one-pixel star would only show in one of the
             * two interlaced fields and flicker. */
            TILE *t = (TILE *) gfx_prim(z, sizeof(TILE));
            if (!t) return;

            int b = bright_base + (int)((h >> 16) & (u32)bright_mask);
            /* Star colours: mostly white, some blue, some amber. */
            int tint = (int) ((h >> 24) & 7);
            int cr = b, cg = b, cb = b;
            if (tint == 0) { cr = b * 3 / 4; cg = b * 7 / 8; }            /* blue-white */
            else if (tint == 1) { cb = b * 5 / 8; cg = b * 7 / 8; }       /* amber */
            setTile(t);
            setXY0(t, sx, sy);
            setWH(t, bright_base >= 170 ? 2 : 1, 2);
            setRGB0(t, cr, cg, cb);
            /* The brightest near stars get a soft halo. */
            if (bright_base >= 170 && ((h >> 16) & 63) > 44) gfx_glow(sx + 1, sy + 1, 9, 9, z, cr / 2, cg / 2, cb / 2);
        }
    }
}

/* Faint clouds of coloured light, far behind the stars: one in some 512-pixel cells. */
static void nebulae(s32 cam_x, s32 cam_y) {
    s32 lx = FROM_FP(cam_x) >> 4, ly = FROM_FP(cam_y) >> 4;
    for (s32 cy = (ly - SCREEN_H / 2 - 260) >> 9; cy <= (ly + SCREEN_H / 2 + 260) >> 9; cy++) {
        for (s32 cx = (lx - SCREEN_W / 2 - 260) >> 9; cx <= (lx + SCREEN_W / 2 + 260) >> 9; cx++) {
            u32 h = hash3(cx, cy, 7 + cur_system);
            if ((h >> 28) < 6) continue;
            int sx = (int) (cx * 512 + (s32) (h & 511) - lx) + SCREEN_W / 2;
            int sy = (int) (cy * 512 + (s32) ((h >> 9) & 511) - ly) + SCREEN_H / 2;
            int rx = 100 + (int) ((h >> 18) & 63), ry = 70 + (int) ((h >> 12) & 31);    /* kept modest: blended pixels are dear */
            static const u8 cols[4][3] = { { 34, 16, 58 }, { 10, 34, 60 }, { 52, 18, 24 }, { 12, 44, 40 } };
            const u8 *c = cols[(h >> 25) & 3];
            gfx_glow(sx, sy, rx, ry, LAYER_STARS_FAR, c[0], c[1], c[2]);
        }
    }
}

void world_draw_stars(s32 cam_x, s32 cam_y) {
    /* far: dim, slow, dense   mid: medium   near: bright, fast, sparse */
    star_layer(cam_x, cam_y, 0, LAYER_STARS_FAR,  3, 5, 12,  50, 31);
    star_layer(cam_x, cam_y, 1, LAYER_STARS_MID,  2, 6,  8, 100, 63);
    star_layer(cam_x, cam_y, 2, LAYER_STARS_NEAR, 1, 6, 10, 170, 63);
    nebulae(cam_x, cam_y);
}

/* ------------------------------------------------------------------------ */
/* Drawing helpers                                                           */
/* ------------------------------------------------------------------------ */

static void box(int x, int y, int w, int h, int r, int g, int b, int z) {
    if (x + w < 0 || x > SCREEN_W || y + h < 0 || y > SCREEN_H)
        return;
    TILE *t = (TILE *) gfx_prim(z, sizeof(TILE));
    if (!t) return;
    setTile(t);
    setXY0(t, x, y);
    setWH(t, w, h);
    setRGB0(t, r, g, b);
}

/* ------------------------------------------------------------------------ */
/* Planet                                                                    */
/* ------------------------------------------------------------------------ */

/* The planet is flown over, not solid: ships draw on top of it. It is drawn as
 * horizontal strips, each cut to the screen. That keeps
 * every rectangle small however far off screen the planet's middle is (the GPU
 * cannot handle coordinates beyond about +-1000). Each strip has a lit part
 * and a dark part (the sun is on the left) and its own band colour. */
#define PLANET_STRIP 6
#define PLANET_ROWS  ((2 * PLANET_MAX_RADIUS) / PLANET_STRIP + 2)
#define PLANET_COLS  5                       /* shading points across each strip edge */

/* For each strip edge (a horizontal line through the disc): its half width,
 * and the light at five points across it (a sphere lit from the upper left,
 * 0..255), and the cloud band there. Worked out once per system. */
static s16 planet_half_width[PLANET_ROWS];
static u8  planet_light[PLANET_ROWS][PLANET_COLS];
static u8  planet_band[PLANET_ROWS];

static void planet_prepare(void) {
    const NavObject *p = &nav_objects[NAV_PLANET];
    int R = p->radius;
    for (int i = 0; i < PLANET_ROWS; i++) {
        int dy = i * PLANET_STRIP - R;
        if (dy > R) dy = R;
        int rem = R * R - dy * dy;
        int hw = rem > 0 ? (int) isqrt32((u32) rem) : 0;
        planet_half_width[i] = (s16) hw;
        for (int k = 0; k < PLANET_COLS; k++) {
            int dx = hw * (k * 2 - (PLANET_COLS - 1)) / (PLANET_COLS - 1);
            int z2 = R * R - dx * dx - dy * dy;
            int nz = z2 > 0 ? (int) isqrt32((u32) z2) : 0;
            int l = (-150 * dx - 125 * dy + 140 * nz) / R;          /* light (-0.6, -0.5, 0.56) x 250 */
            if (l < 0) l = 0;
            l = l * 255 / 235;
            planet_light[i][k] = (u8) (l > 255 ? 255 : l);
        }
        u32 h = hash3(i / 2, cur_system, 31);
        planet_band[i] = (u8) (200 + (h & 31) + ((i % 5) == 0 ? 24 : 0));       /* 200..255 */
    }
    planet_ready = 1;
}

static u32 planet_colour(int row, int k) {
    int l = planet_light[row][k], band = planet_band[row];
    int r = planet_rgb[0] * band >> 8, g = planet_rgb[1] * band >> 8, b = planet_rgb[2] * band >> 8;
    /* Lit colour, pushed toward white at full light; a cold glimmer on the night side. */
    r = (r * l * 5 >> 10) + (l > 200 ? (l - 200) : 0) + 3;
    g = (g * l * 5 >> 10) + (l > 200 ? (l - 200) : 0) + 4;
    b = (b * l * 5 >> 10) + (l > 200 ? (l - 200) : 0) + 9;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return (u32) r << 16 | (u32) g << 8 | (u32) b;
}

static void draw_planet(s32 cam_x, s32 cam_y) {
    const NavObject *p = &nav_objects[NAV_PLANET];
    int pcx = SCREEN_W / 2 + p->x - (int) FROM_FP(cam_x);
    int pcy = SCREEN_H / 2 + p->y - (int) FROM_FP(cam_y);
    int R = p->radius;

    if (pcx + R + 40 < 0 || pcx - R - 40 > SCREEN_W || pcy + R + 40 < 0 || pcy - R - 40 > SCREEN_H)
        return;
    if (!planet_ready) planet_prepare();

    /* The disc: Gouraud-shaded strips, each edge following the limb. */
    int rows = 2 * R / PLANET_STRIP;
    for (int row = 0; row <= rows; row++) {
        int y0 = pcy - R + row * PLANET_STRIP, y1 = y0 + PLANET_STRIP;
        if (y1 > pcy + R) y1 = pcy + R;
        if (y1 <= 0 || y0 >= SCREEN_H || y1 <= y0) continue;
        int hw0 = planet_half_width[row], hw1 = planet_half_width[row + 1];
        for (int k = 0; k < PLANET_COLS - 1; k++) {
            int xa0 = pcx + hw0 * (k * 2 - (PLANET_COLS - 1)) / (PLANET_COLS - 1);
            int xb0 = pcx + hw0 * (k * 2 + 2 - (PLANET_COLS - 1)) / (PLANET_COLS - 1);
            int xa1 = pcx + hw1 * (k * 2 - (PLANET_COLS - 1)) / (PLANET_COLS - 1);
            int xb1 = pcx + hw1 * (k * 2 + 2 - (PLANET_COLS - 1)) / (PLANET_COLS - 1);
            if ((xb0 < 0 && xb1 < 0) || (xa0 > SCREEN_W && xa1 > SCREEN_W)) continue;
            POLY_G4 *q = (POLY_G4 *) gfx_prim(LAYER_PLANET, sizeof(POLY_G4));
            if (!q) return;
            u32 c0 = planet_colour(row, k), c1 = planet_colour(row, k + 1);
            u32 c2 = planet_colour(row + 1, k), c3 = planet_colour(row + 1, k + 1);
            setPolyG4(q);
            setXY4(q, xa0, y0, xb0, y0, xa1, y1, xb1, y1);
            setRGB0(q, c0 >> 16, c0 >> 8, c0); setRGB1(q, c1 >> 16, c1 >> 8, c1);
            setRGB2(q, c2 >> 16, c2 >> 8, c2); setRGB3(q, c3 >> 16, c3 >> 8, c3);
        }
    }

    /* Atmosphere: a ring of light just outside the limb, brightest toward the sun. */
    for (int i = 0; i < 24; i++) {
        int a0 = i * (ANGLE_FULL / 24), a1 = (i + 1) * (ANGLE_FULL / 24);
        int x0 = pcx + (isin(a0) * R >> FP_SHIFT), y0 = pcy - (icos(a0) * R >> FP_SHIFT);
        int x1 = pcx + (isin(a1) * R >> FP_SHIFT), y1 = pcy - (icos(a1) * R >> FP_SHIFT);
        int ro = R + 10 + R / 10;
        int x2 = pcx + (isin(a0) * ro >> FP_SHIFT), y2 = pcy - (icos(a0) * ro >> FP_SHIFT);
        int x3 = pcx + (isin(a1) * ro >> FP_SHIFT), y3 = pcy - (icos(a1) * ro >> FP_SHIFT);
        if ((x0 < 0 && x2 < 0) || (x0 > SCREEN_W && x2 > SCREEN_W) || (y0 < 0 && y2 < 0) || (y0 > SCREEN_H && y2 > SCREEN_H)) continue;
        /* Sunward (upper left, about 10 o'clock) is brightest. */
        int f0 = 60 + ((-isin(a0) * 5 + icos(a0) * 4) * 22 >> FP_SHIFT), f1 = 60 + ((-isin(a1) * 5 + icos(a1) * 4) * 22 >> FP_SHIFT);
        if (f0 < 14) f0 = 14;
        if (f1 < 14) f1 = 14;
        u32 c0 = (u32) (planet_rgb[0] * f0 >> 8) << 16 | (u32) (planet_rgb[1] * f0 >> 8) << 8 | (u32) ((planet_rgb[2] + 60) * f0 >> 8);
        u32 c1 = (u32) (planet_rgb[0] * f1 >> 8) << 16 | (u32) (planet_rgb[1] * f1 >> 8) << 8 | (u32) ((planet_rgb[2] + 60) * f1 >> 8);
        gfx_tri_g(x0, y0, x1, y1, x2, y2, LAYER_PLANET, c0, c1, 0, 1);
        gfx_tri_g(x1, y1, x3, y3, x2, y2, LAYER_PLANET, c1, 0, 0, 1);
    }
}

/* ------------------------------------------------------------------------ */
/* Station                                                                   */
/* ------------------------------------------------------------------------ */

static void draw_station(int sx, int sy, int frame) {
    if (sx < -90 || sx > SCREEN_W + 90 || sy < -90 || sy > SCREEN_H + 90) return;

    int spin = (frame * 4) & ANGLE_MASK;   /* one turn in about 17 seconds */
    /* Running lights first (they sit on top), then the 3D model seen from above. */
    int pulse = (frame >> 3) & 7;
    gfx_glow(sx, sy, 12 + pulse, 12 + pulse, LAYER_OBJECTS - 1, 90, 200, 255);
    for (int i = 0; i < 4; i++) {
        int a = (spin + 512 + i * 1024) & ANGLE_MASK;
        int lx = sx + (isin(a) * 47 >> FP_SHIFT), ly = sy - (icos(a) * 47 >> FP_SHIFT);
        if (((frame >> 4) + i) & 1) gfx_glow(lx, ly, 9, 9, LAYER_OBJECTS - 1, i & 1 ? 255 : 90, i & 1 ? 60 : 220, i & 1 ? 50 : 255);
    }
    model_draw(MESH_STATION, sx, sy, 68, spin, 1024, LAYER_OBJECTS);
}

/* ------------------------------------------------------------------------ */
/* All objects                                                               */
/* ------------------------------------------------------------------------ */

/* Just the planet, with its centre (x, y) pixels from the middle of the screen. */
void world_draw_planet(s32 off_x, s32 off_y) {
    const NavObject *p = &nav_objects[NAV_PLANET];
    draw_planet(TO_FP(p->x) - off_x, TO_FP(p->y) - off_y);
}

void world_draw_objects(s32 cam_x, s32 cam_y, int frame) {
    s32 cx = FROM_FP(cam_x);
    s32 cy = FROM_FP(cam_y);
    if (nav_count == 0) return;            /* empty space: no planet, no station, no buoys */

    draw_planet(cam_x, cam_y);

    const NavObject *st = &nav_objects[NAV_STATION];
    draw_station(SCREEN_W / 2 + st->x - cx, SCREEN_H / 2 + st->y - cy, frame);

    /* Beacons: at most one per 256-pixel cell, blinking every 32 frames. */
    s32 c0x = (cx - SCREEN_W / 2 - 16) >> 8;
    s32 c1x = (cx + SCREEN_W / 2 + 16) >> 8;
    s32 c0y = (cy - SCREEN_H / 2 - 16) >> 8;
    s32 c1y = (cy + SCREEN_H / 2 + 16) >> 8;
    int blink = (frame >> 5) & 1;

    for (s32 gy = c0y; gy <= c1y; gy++) {
        for (s32 gx = c0x; gx <= c1x; gx++) {
            u32 h = hash3(gx, gy, 99);
            if ((int)(h >> 28) >= 4)
                continue;   /* about one cell in four holds a beacon */

            s32 wx = gx * 256 + 16 + (s32)(h & 0xBF);
            s32 wy = gy * 256 + 16 + (s32)((h >> 8) & 0xBF);
            int sx = (int)(wx - cx) + SCREEN_W / 2;
            int sy = (int)(wy - cy) + SCREEN_H / 2;

            if (sx < -12 || sx > SCREEN_W + 12 || sy < -12 || sy > SCREEN_H + 12) continue;
            /* A nav buoy: a little steel diamond with a red lamp. */
            if (blink) gfx_glow(sx, sy, 10, 10, LAYER_OBJECTS - 1, 255, 70, 50);
            gfx_tri_g(sx, sy - 5, sx + 5, sy, sx - 5, sy, LAYER_OBJECTS, 0xA0AABE, 0x6A7488, 0x8A94A8, 0);
            gfx_tri_g(sx - 5, sy, sx + 5, sy, sx, sy + 5, LAYER_OBJECTS, 0x6A7488, 0x4A5264, 0x30384A, 0);
        }
    }
}
