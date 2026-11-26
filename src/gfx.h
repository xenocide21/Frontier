/*
 * FRONTIER - graphics: double-buffered display, ordering table, primitive pool.
 *
 * Follows the structure of the SDK's template example. Layers are numbers
 * 0..15; LOWER numbers are drawn LAST, so layer 0 is in front.
 */
#ifndef FRONTIER_GFX_H
#define FRONTIER_GFX_H

#include <stddef.h>
#include "common.h"

#define OT_LENGTH 16
#define PRIM_BUFFER_SIZE 98304   /* bytes per frame buffer */

enum {
    LAYER_HUD        = 0,
    LAYER_BARS       = 1,
    LAYER_PARTICLES  = 2,
    LAYER_PROJECTILE = 3,
    LAYER_SHIP_TOP   = 4,
    LAYER_SHIP       = 5,
    LAYER_SHIP_UNDER = 6,
    LAYER_BARS_BG    = 7,
    LAYER_OBJECTS    = 8,
    LAYER_STARS_NEAR = 11,
    LAYER_STARS_MID  = 12,
    LAYER_STARS_FAR  = 13
};

void gfx_init(void);

/* Ends the frame: waits for the GPU and vertical sync, then swaps buffers. */
void gfx_flip(void);

/* Returns zeroed-size space for one primitive, already linked into layer z.
 * Returns NULL (and counts an overflow) if the frame's pool is full. */
void *gfx_prim(int z, size_t size);

/* Draws text with the SDK's built-in font. Truncated if the pool is nearly full. */
void gfx_text(int x, int y, int z, const char *text);

/* Scaled text for menus: each 8x8 glyph of the SDK font is drawn as a textured
 * quad (8*scale pixels square) in the given colour. Needs gfx_init() first. */
void gfx_text_big(int x, int y, int z, int scale, int r, int g, int b, const char *text);

/* A plain filled rectangle. Later additions to a layer are drawn underneath
 * earlier ones, so add foreground shapes first. */
void gfx_rect(int x, int y, int w, int h, int z, int r, int g, int b);

/* A filled rectangle with a one-pixel-thick (two with scale) border, as a menu
 * panel. Add the panel AFTER the text that sits on it (the last primitive added
 * to a layer is drawn first). */
void gfx_panel(int x, int y, int w, int h, int z);
void gfx_tri(int x0, int y0, int x1, int y1, int x2, int y2, int z, int r, int g, int b);

/* Milestone 14e. Colours here are 0xRRGGBB. */
enum { GFX_HALF = 0, GFX_ADD, GFX_SUB, GFX_ADD_QUARTER };
int  gfx_text_width(int scale, const char *text);
void gfx_grad(int x, int y, int w, int h, int z, u32 top, u32 bottom);          /* Gouraud, top to bottom */
void gfx_grad_h(int x, int y, int w, int h, int z, u32 left, u32 right);
void gfx_blend(int x, int y, int w, int h, int z, int mode, u32 top, u32 bottom);   /* see-through */
void gfx_tri_g(int x0, int y0, int x1, int y1, int x2, int y2, int z, u32 c0, u32 c1, u32 c2, int add);
void gfx_line(int x0, int y0, int x1, int y1, int z, u32 c0, u32 c1, int add);
void gfx_glow(int cx, int cy, int rx, int ry, int z, int r, int g, int b);      /* soft additive light */

/* A button: label at the given text scale, left-aligned, on a bordered box.
 * state: BTN_NORMAL, BTN_SELECTED (highlighted) or BTN_DISABLED (greyed).
 * Call it BEFORE the panel behind it is added, like any foreground shape
 * (the label goes in first, then the box, so the box ends up underneath). */
enum { BTN_NORMAL = 0, BTN_SELECTED, BTN_DISABLED, BTN_MARKED };   /* MARKED: normal, cyan text */
void gfx_button(int x, int y, int w, int h, int z, int state, int scale, const char *label);

/* Statistics for the on-screen HUD. */
/* A framed card (button colours, no text): add it AFTER what goes on it in the
 * same layer, because primitives in one layer draw in reverse order. */
void gfx_card(int x, int y, int w, int h, int z);

/* A horizontal meter: value out of max, filled in r, g, b over a dark track. */
void gfx_bar(int x, int y, int w, int h, int z, int value, int max, int r, int g, int b);

int gfx_prim_bytes_used(void);
int gfx_prim_overflows(void);

#endif
