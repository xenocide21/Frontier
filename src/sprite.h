/*
 * FRONTIER - textured sprites (graphics overhaul, stage 1).
 *
 * Each ship sprite is a square 16-colour (4-bit) top-down picture rendered
 * from a 3D model (tools/shipgen) and converted to PS1 data in sprites_data.c:
 * 64 px for fighters and civilian ships (2 KB), 128 px for corvettes and
 * frigates (8 KB), 256 px for destroyers and cruisers (32 KB). They are packed
 * into texture pages in VRAM x 640..1023 by the converter; their palettes
 * (CLUTs) sit on the free rows under the frame buffer, y 480 and on.
 *
 * Ships are drawn as one textured quad turned by the GPU, so a sprite needs only
 * one picture rather than one per heading. Colour index 0 is transparent.
 */
#ifndef FRONTIER_SPRITE_H
#define FRONTIER_SPRITE_H

#include "common.h"

typedef struct {
    const u8  *pix;      /* size x size, 4 bits per pixel, left pixel in the low nibble */
    const u16 *clut;     /* 16 PS1 colours; entry 0 = transparent */
    const char *name;
    u16 size;            /* 64, 128 or 256 */
    u16 page_x, page_y;  /* texture page corner in VRAM */
    u8  u, v;            /* the sprite's corner inside the page */
} SpriteSrc;

int sprite_size(int id);       /* pixels, or 0 for no sprite */

#include "sprites_data.h"      /* SPRITE_COUNT, SPR_* ids, sprite_srcs[] */

void sprites_init(void);       /* uploads every sprite to VRAM (call after gfx_init) */

/* Draws sprite id centred on (cx, cy), turned to heading angle (0 = nose up,
 * 1024 = right), scaled by scale_pct (100 = 1:1, its own size in pixels). r, g, b tint it: 128 = as
 * drawn, 255 = twice as bright (hit flash). */
void sprite_draw(int id, int cx, int cy, int angle, int scale_pct, int z, int r, int g, int b);

#endif
