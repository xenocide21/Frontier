#include "place.h"
#include "model.h"
#include "galaxy.h"
#include "gfx.h"
#include "world.h"

/* [system][0] = its station, [1] = its planet: generated with the galaxy (src/galaxy_data.c). */
extern const Place galaxy_places[GALAXY_SYSTEMS][2];

const Place *place_for_nav(int nav_index) {
    if (nav_index < 0 || nav_index > 1) return 0;
    if (galaxy_empty(galaxy_current())) return 0;
    return &galaxy_places[galaxy_current()][nav_index];
}

/* Foreground shapes first: the last rectangle added to a layer is drawn first.
 * Pictures are laid out for a 180 x 110 box and scale to whatever box is given. */
#define SX(v) ((v) * w / 180)
#define SY(v) ((v) * h / 110)

/* The view from the dock (Milestone 14e): your own ship, in 3D, on the pad.
 * Remember: in a layer the last thing added is drawn first, so the ship and the
 * lights go in before the walls behind them. */
static int art_mesh = 0;
void place_set_ship(int mesh) { art_mesh = mesh; }

/* A station hangar: steel walls, strip lights, your ship under the lamps. */
static void art_station(int x, int y, int w, int h, int z, int frame) {
    int fy = y + h * 2 / 3;                                   /* where the wall meets the deck */
    for (int i = 0; i < 5; i++) {                              /* ceiling lamps, one flickering */
        int lx = x + 22 + i * (w - 44) / 4;
        if (i != 3 || (frame & 31) > 2) {
            gfx_glow(lx, y + 9, 20, 9, z, 150, 220, 255);
            gfx_rect(lx - 9, y + 7, 18, 3, z, 220, 245, 255);
        }
    }
    for (int i = 0; i < 6; i++)                                /* guide lights chasing along the deck */
        if (((frame >> 3) + i) % 6 < 2) gfx_glow(x + 18 + i * (w - 36) / 5, y + h - 9, 7, 4, z, 255, 190, 60);
    gfx_glow(x + w / 2, fy - 6, w / 3, 14, z, 40, 80, 130);    /* light pooling under the ship */
    model_draw(art_mesh, x + w / 2, y + h / 2 + 6, h * 2 / 5, (frame * 5) & 4095, 330, z);
    gfx_rect(x, fy, w, 1, z, 70, 110, 160);
    for (int i = 1; i < 6; i++) gfx_rect(x + i * w / 6, y, 1, fy - y, z, 14, 22, 40);     /* wall ribs */
    gfx_grad(x, fy, w, y + h - fy, z, 0x1E2C46, 0x0A0E18);     /* deck */
    gfx_grad(x, y, w, fy - y, z, 0x0A1224, 0x1A2A48);          /* wall */
}

/* A planet's spaceport: dusk sky, a far skyline, your ship on the apron. */
static void art_spaceport(int x, int y, int w, int h, int z, int frame) {
    int hy = y + h * 3 / 5;                                    /* horizon */
    static const u8 towers[10] = { 12, 26, 18, 40, 22, 14, 34, 20, 28, 10 };
    for (int i = 0; i < 6; i++)
        if (((frame >> 3) + i) % 6 < 2) gfx_glow(x + 18 + i * (w - 36) / 5, y + h - 8, 7, 4, z, 120, 255, 160);
    model_draw(art_mesh, x + w / 2, y + h / 2 + 12, h * 2 / 5, (frame * 5) & 4095, 330, z);
    gfx_glow(x + w / 2, y + h - 22, w / 3, 12, z, 60, 50, 40);
    for (int i = 0; i < 10; i++) {                             /* the skyline, with a few lit windows */
        int tx = x + 4 + i * (w - 8) / 10, th = towers[i];
        if ((i * 7 + (frame >> 6)) % 3 == 0) gfx_rect(tx + 4, hy - th + 5, 2, 2, z, 255, 220, 120);
        if (i % 3 == 1 && (frame >> 4) & 1) gfx_glow(tx + 8, hy - th, 5, 5, z, 255, 60, 40);
        gfx_grad(tx, hy - th, (w - 8) / 10 - 3, th, z, 0x1A1630, 0x0C0A18);
    }
    gfx_glow(x + w * 3 / 4, hy - 4, 46, 30, z, 255, 150, 70);  /* the sun going down */
    gfx_grad(x, hy, w, y + h - hy, z, 0x2A2430, 0x0E0C12);     /* apron */
    gfx_grad(x, y, w, hy - y, z, 0x0A1030, 0x7A3A46);          /* sky: night blue to ember */
}

void place_draw_art(const Place *p, int x, int y, int w, int h, int z, int frame) {
    if (!p) return;
    if (p->art == ART_STATION) art_station(x, y, w, h, z, frame);
    else                       art_spaceport(x, y, w, h, z, frame);
    /* Frame around the picture, added last so it is drawn first (underneath). */
    gfx_rect(x - 2, y - 2, w + 4, h + 4, z, 90, 120, 160);
}
