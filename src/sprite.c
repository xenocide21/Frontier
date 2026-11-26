#include <psxgpu.h>
#include <psxgte.h>

#include "gfx.h"
#include "sprite.h"

#define CLUT_Y    480        /* palettes: free rows under the 640x480 frame buffer */

static u16 tpages[SPRITE_COUNT + 1];
static u16 cluts[SPRITE_COUNT + 1];

int sprite_size(int id) { return (id >= 0 && id < SPRITE_COUNT) ? sprite_srcs[id].size : 0; }

void sprites_init(void) {
    for (int k = 0; k < SPRITE_COUNT; k++) {
        const SpriteSrc *sp = &sprite_srcs[k];
        RECT r;
        r.x = (int16_t) (sp->page_x + sp->u / 4);    /* 4 pixels per VRAM halfword */
        r.y = (int16_t) (sp->page_y + sp->v);
        r.w = (int16_t) (sp->size / 4);
        r.h = (int16_t) sp->size;
        LoadImage(&r, (const uint32_t *) sp->pix);
        DrawSync(0);

        int cx = (16 * k) % 640, cy = CLUT_Y + (16 * k) / 640;
        RECT c = { (int16_t) cx, (int16_t) cy, 16, 1 };
        LoadImage(&c, (const uint32_t *) sp->clut);
        DrawSync(0);
        cluts[k] = getClut(cx, cy);
        tpages[k] = getTPage(0, 0, sp->page_x, sp->page_y);   /* 4-bit colour */
    }
}

void sprite_draw(int id, int cx, int cy, int angle, int scale_pct, int z, int r, int g, int b) {
    if (id < 0 || id >= SPRITE_COUNT) return;
    const SpriteSrc *sp = &sprite_srcs[id];
    int size = sp->size;
    int h = size / 2 * scale_pct / 100;
    if (cx < -h - 8 || cx > SCREEN_W + h + 8 || cy < -h - 8 || cy > SCREEN_H + h + 8) return;

    POLY_FT4 *q = (POLY_FT4 *) gfx_prim(z, sizeof(POLY_FT4));
    if (!q) return;

    s32 sn = isin(angle & ANGLE_MASK), cs = icos(angle & ANGLE_MASK);
    /* Corner (px, py), y down, turned clockwise by the heading. */
    #define RX(px, py) (cx + (int) (((px) * cs - (py) * sn) >> FP_SHIFT))
    #define RY(px, py) (cy + (int) (((px) * sn + (py) * cs) >> FP_SHIFT))
    setPolyFT4(q);
    setXY4(q, RX(-h, -h), RY(-h, -h), RX(h, -h), RY(h, -h),
              RX(-h,  h), RY(-h,  h), RX(h,  h), RY(h,  h));
    #undef RX
    #undef RY

    int u = sp->u, v = sp->v;
    setUV4(q, u, v, u + size - 1, v, u, v + size - 1, u + size - 1, v + size - 1);
    setRGB0(q, r, g, b);
    q->tpage = tpages[id];
    q->clut = cluts[id];
}
