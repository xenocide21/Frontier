/*
 * FRONTIER - the graphics core. Rewritten in Milestone 14e (the graphics pass):
 *   - the FRONTIER typeface in three native sizes (font_data.c), anti-aliased,
 *     with a baked drop shadow; headings get a Gouraud-shaded sheen;
 *   - Gouraud gradients, with the GPU's dithering switched on;
 *   - semi-transparent and additive primitives (glass panels, glows, beams);
 *   - frame pacing that keeps interlace intact when a frame runs long.
 */
#include <ctype.h>
#include <string.h>
#include <psxgpu.h>

#include "gfx.h"

typedef struct {
    uint32_t ot[OT_LENGTH];
    uint8_t  buffer[PRIM_BUFFER_SIZE];
} RenderBuffer;

typedef struct {
    RenderBuffer buffers[2];
    DISPENV      disp_env;
    DRAWENV      draw_env;
    uint8_t     *next_packet;
    int          active_buffer;
    int          overflows;
} RenderContext;

static RenderContext ctx;
static int last_draw_vbl;

#ifdef FRONTIER_HOST
void host_add_prim(int z, void *p);
void host_present(int dither);
#endif

/* Texture space (VRAM x 640..1023): ship sprites fill nine pages (sprite.c).
 *   (896,256) large font, small font       (960,256) medium font, glow
 *   (960,0)   the title logo (logo.c)
 * Palettes sit under the frame buffer: row 480 sprites, 481 font and glow, 482 logo. */
#define PAGE_A_X 896
#define PAGE_A_Y 256
#define PAGE_B_X 960
#define PAGE_B_Y 256
#define SMALL_V  184
#define GLOW_V   96

extern const u16 font_small_pix[], font_medium_pix[], font_large_pix[], font_clut[], glow_pix[], glow_clut[];
extern const u16 font_small_wh[2], font_medium_wh[2], font_large_wh[2];
static u16 tp_a, tp_b, tp_glow, clut_font, clut_glow;

/*
 * 640x480 interlaced uses the whole left part of video memory, so there is only
 * ONE frame buffer. With "draw to display area" blocked (dfe = 0) the GPU only
 * draws on the lines of the field that is NOT being shown, so each frame
 * redraws every second line. The background is a full-screen primitive (the
 * SDK's fill would wipe the shown field too).
 */
static void add_background(void) {
    POLY_G4 *bg = (POLY_G4 *) gfx_prim(OT_LENGTH - 1, sizeof(POLY_G4));
    if (!bg) return;
    setPolyG4(bg);
    setXY4(bg, 0, 0, SCREEN_W, 0, 0, SCREEN_H, SCREEN_W, SCREEN_H);
    setRGB0(bg, 1, 2, 9); setRGB1(bg, 3, 2, 12);         /* deep space: a faint violet drift */
    setRGB2(bg, 2, 4, 14); setRGB3(bg, 1, 1, 6);
}

static void upload(int x, int y, const u16 *wh, const u16 *pix) {
    RECT r = { (int16_t) x, (int16_t) y, (int16_t) wh[0], (int16_t) wh[1] };
    LoadImage(&r, (const uint32_t *) pix);
    DrawSync(0);
}

void gfx_init(void) {
    ResetGraph(0);

    SetDefDrawEnv(&ctx.draw_env, 0, 0, SCREEN_W, SCREEN_H);
    SetDefDispEnv(&ctx.disp_env, 0, 0, SCREEN_W, SCREEN_H);
    ctx.draw_env.dfe = 0;        /* never draw on the field being displayed */
    ctx.draw_env.isbg = 0;
    ctx.draw_env.dtd = 1;        /* dithering: smooth gradients in 15-bit colour */
    ctx.disp_env.isinter = 1;

    static const u16 wh16[2] = { 16, 1 }, glow_wh[2] = { 16, 64 };
    upload(PAGE_A_X, PAGE_A_Y, font_large_wh, font_large_pix);
    upload(PAGE_A_X, PAGE_A_Y + SMALL_V, font_small_wh, font_small_pix);
    upload(PAGE_B_X, PAGE_B_Y, font_medium_wh, font_medium_pix);
    upload(PAGE_B_X, PAGE_B_Y + GLOW_V, glow_wh, glow_pix);
    upload(0, 481, wh16, font_clut);
    upload(16, 481, wh16, glow_clut);
    tp_a = getTPage(0, 0, PAGE_A_X, PAGE_A_Y);
    tp_b = getTPage(0, 0, PAGE_B_X, PAGE_B_Y);
    tp_glow = getTPage(0, 1, PAGE_B_X, PAGE_B_Y);          /* additive */
    clut_font = getClut(0, 481);
    clut_glow = getClut(16, 481);

    ctx.active_buffer = 0;
    ctx.next_packet   = ctx.buffers[0].buffer;
    ctx.overflows     = 0;
    ClearOTagR(ctx.buffers[0].ot, OT_LENGTH);
    add_background();

    PutDispEnv(&ctx.disp_env);
    SetDispMask(1);
}

void gfx_flip(void) {
    DrawSync(0);
    VSync(0);
    /* Interlace: each frame must land on the field opposite to the last one.
     * If this frame took an even number of fields, wait one more, or half the
     * lines would stop updating. (60, 20, 12 fps are fine; 30 is not.) */
    int now = VSync(-1);
    if (last_draw_vbl && ((now - last_draw_vbl) & 1) == 0) { VSync(0); now = VSync(-1); }
    last_draw_vbl = now;

    RenderBuffer *draw_buffer = &ctx.buffers[ctx.active_buffer];
#ifdef FRONTIER_HOST
    (void) draw_buffer;
    host_present(ctx.draw_env.dtd);
#else
    PutDispEnv(&ctx.disp_env);
    DrawOTagEnv(&draw_buffer->ot[OT_LENGTH - 1], &ctx.draw_env);
#endif

    ctx.active_buffer ^= 1;
    RenderBuffer *next_buffer = &ctx.buffers[ctx.active_buffer];
    ctx.next_packet = next_buffer->buffer;
    ctx.overflows   = 0;
    ClearOTagR(next_buffer->ot, OT_LENGTH);
    add_background();
}

void *gfx_prim(int z, size_t size) {
    RenderBuffer *buffer = &ctx.buffers[ctx.active_buffer];
    uint8_t *end = &buffer->buffer[PRIM_BUFFER_SIZE];

    if (z < 0) z = 0;
    if (z >= OT_LENGTH) z = OT_LENGTH - 1;
    if (ctx.next_packet + size > end) { ctx.overflows++; return NULL; }

    uint8_t *prim = ctx.next_packet;
#ifdef FRONTIER_HOST
    host_add_prim(z, prim);
#else
    addPrim(&buffer->ot[z], prim);
#endif
    ctx.next_packet += size;
    return prim;
}

/* Sets the GPU's blend mode and texture page for the untextured primitives and
 * sprites added just BEFORE this call (a layer draws in reverse order). */
static void mode_prim(int z, u16 tpage) {
    DR_TPAGE *t = (DR_TPAGE *) gfx_prim(z, sizeof(DR_TPAGE));
    if (t) { setDrawTPage(t, 0, 1, tpage); }
}

/* ------------------------------------------------------------------------ */
/* Text                                                                      */
/* ------------------------------------------------------------------------ */

static int clamp255(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }

void gfx_text_big(int x, int y, int z, int scale, int r, int g, int b, const char *text) {
    int size = 8 * scale, sx = x, sy = y, any = 0;
    /* Atlas cell for this size: small 8x9 (16 a row), medium 16x18 (15), large 24x26 (10). */
    int cw = scale == 1 ? 8 : scale == 2 ? 16 : 24, ch = scale == 1 ? 9 : scale == 2 ? 18 : 26;
    int cols = scale == 1 ? 16 : scale == 2 ? 15 : 10, v0 = scale == 1 ? SMALL_V : 0;
    int dw = scale <= 3 ? cw : cw * scale / 3, dh = scale <= 3 ? ch : ch * scale / 3;
    /* Headings: lighter at the top, deeper at the foot (128 = the texture's own colour). */
    int tr = clamp255(r + (255 - r) * 3 / 8) >> 1, tg = clamp255(g + (255 - g) * 3 / 8) >> 1, tb = clamp255(b + (255 - b) * 3 / 8) >> 1;
    int br = r * 3 / 8, bg = g * 3 / 8, bb = b * 3 / 8;

    for (; *text; text++) {
        if (*text == '\n') { sx = x; sy += size + 2; continue; }
        int i = toupper((unsigned char) *text) - ' ';
        if (i > 0 && i < 64) {
            int u = (i % cols) * cw, v = v0 + (i / cols) * ch;
            if (scale == 1) {
                SPRT *s = (SPRT *) gfx_prim(z, sizeof(SPRT));
                if (!s) break;
                setSprt(s);
                setXY0(s, sx, sy);
                setWH(s, cw, ch);
                setUV0(s, u, v);
                setRGB0(s, r >> 1, g >> 1, b >> 1);
                s->clut = clut_font;
                any = 1;
            } else {
                POLY_GT4 *q = (POLY_GT4 *) gfx_prim(z, sizeof(POLY_GT4));
                if (!q) return;
                setPolyGT4(q);
                setXY4(q, sx, sy, sx + dw, sy, sx, sy + dh, sx + dw, sy + dh);
                setUV4(q, u, v, u + cw, v, u, v + ch, u + cw, v + ch);
                setRGB0(q, tr, tg, tb); setRGB1(q, tr, tg, tb);
                setRGB2(q, br, bg, bb); setRGB3(q, br, bg, bb);
                q->tpage = scale == 2 ? tp_b : tp_a;
                q->clut = clut_font;
            }
        }
        sx += size;
    }
    if (any) mode_prim(z, tp_a);
}

void gfx_text(int x, int y, int z, const char *text) {
    gfx_text_big(x, y, z, 1, 226, 232, 244, text);
}

int gfx_text_width(int scale, const char *text) { return (int) strlen(text) * 8 * scale; }

/* ------------------------------------------------------------------------ */
/* Shapes                                                                    */
/* ------------------------------------------------------------------------ */

void gfx_rect(int x, int y, int w, int h, int z, int r, int g, int b) {
    TILE *t = (TILE *) gfx_prim(z, sizeof(TILE));
    if (!t) return;
    setTile(t);
    setXY0(t, x, y);
    setWH(t, w, h);
    setRGB0(t, r, g, b);
}

void gfx_grad(int x, int y, int w, int h, int z, u32 top, u32 bottom) {
    POLY_G4 *p = (POLY_G4 *) gfx_prim(z, sizeof(POLY_G4));
    if (!p) return;
    setPolyG4(p);
    setXY4(p, x, y, x + w, y, x, y + h, x + w, y + h);
    setRGB0(p, top >> 16, top >> 8, top); setRGB1(p, top >> 16, top >> 8, top);
    setRGB2(p, bottom >> 16, bottom >> 8, bottom); setRGB3(p, bottom >> 16, bottom >> 8, bottom);
}

void gfx_grad_h(int x, int y, int w, int h, int z, u32 left, u32 right) {
    POLY_G4 *p = (POLY_G4 *) gfx_prim(z, sizeof(POLY_G4));
    if (!p) return;
    setPolyG4(p);
    setXY4(p, x, y, x + w, y, x, y + h, x + w, y + h);
    setRGB0(p, left >> 16, left >> 8, left); setRGB2(p, left >> 16, left >> 8, left);
    setRGB1(p, right >> 16, right >> 8, right); setRGB3(p, right >> 16, right >> 8, right);
}

/* A see-through rectangle. mode: GFX_HALF (50/50), GFX_ADD, GFX_SUB, GFX_ADD_QUARTER. */
void gfx_blend(int x, int y, int w, int h, int z, int mode, u32 top, u32 bottom) {
    POLY_G4 *p = (POLY_G4 *) gfx_prim(z, sizeof(POLY_G4));
    if (!p) return;
    setPolyG4(p);
    setSemiTrans(p, 1);
    setXY4(p, x, y, x + w, y, x, y + h, x + w, y + h);
    setRGB0(p, top >> 16, top >> 8, top); setRGB1(p, top >> 16, top >> 8, top);
    setRGB2(p, bottom >> 16, bottom >> 8, bottom); setRGB3(p, bottom >> 16, bottom >> 8, bottom);
    mode_prim(z, getTPage(0, mode, PAGE_A_X, PAGE_A_Y));
}

void gfx_tri(int x0, int y0, int x1, int y1, int x2, int y2, int z, int r, int g, int b) {
    POLY_F3 *p = (POLY_F3 *) gfx_prim(z, sizeof(POLY_F3));
    if (!p) return;
    setPolyF3(p);
    setXY3(p, x0, y0, x1, y1, x2, y2);
    setRGB0(p, r, g, b);
}

void gfx_tri_g(int x0, int y0, int x1, int y1, int x2, int y2, int z, u32 c0, u32 c1, u32 c2, int add) {
    POLY_G3 *p = (POLY_G3 *) gfx_prim(z, sizeof(POLY_G3));
    if (!p) return;
    setPolyG3(p);
    if (add) setSemiTrans(p, 1);
    setXY3(p, x0, y0, x1, y1, x2, y2);
    setRGB0(p, c0 >> 16, c0 >> 8, c0); setRGB1(p, c1 >> 16, c1 >> 8, c1); setRGB2(p, c2 >> 16, c2 >> 8, c2);
    if (add) mode_prim(z, getTPage(0, GFX_ADD, PAGE_A_X, PAGE_A_Y));
}

void gfx_line(int x0, int y0, int x1, int y1, int z, u32 c0, u32 c1, int add) {
    LINE_G2 *l = (LINE_G2 *) gfx_prim(z, sizeof(LINE_G2));
    if (!l) return;
    setLineG2(l);
    if (add) setSemiTrans(l, 1);
    setXY2(l, x0, y0, x1, y1);
    setRGB0(l, c0 >> 16, c0 >> 8, c0); setRGB1(l, c1 >> 16, c1 >> 8, c1);
    if (add) mode_prim(z, getTPage(0, GFX_ADD, PAGE_A_X, PAGE_A_Y));
}

/* A soft round glow, added to what is behind it. rx, ry: half sizes in pixels. */
void gfx_glow(int cx, int cy, int rx, int ry, int z, int r, int g, int b) {
    if (cx + rx < 0 || cx - rx > SCREEN_W || cy + ry < 0 || cy - ry > SCREEN_H) return;
    POLY_FT4 *q = (POLY_FT4 *) gfx_prim(z, sizeof(POLY_FT4));
    if (!q) return;
    setPolyFT4(q);
    setSemiTrans(q, 1);
    setXY4(q, cx - rx, cy - ry, cx + rx, cy - ry, cx - rx, cy + ry, cx + rx, cy + ry);
    setUV4(q, 0, GLOW_V, 64, GLOW_V, 0, GLOW_V + 64, 64, GLOW_V + 64);
    setRGB0(q, r >> 1, g >> 1, b >> 1);
    q->tpage = tp_glow;
    q->clut = clut_glow;
}

/* ------------------------------------------------------------------------ */
/* Interface pieces. Within a layer the LAST thing added is drawn FIRST, so  */
/* each of these adds its top details first and its backing last, and you    */
/* add a panel AFTER the text that sits on it.                               */
/* ------------------------------------------------------------------------ */

/* Bright corner brackets on a frame. */
static void corners(int x, int y, int w, int h, int z, int len, int r, int g, int b) {
    gfx_rect(x, y, len, 2, z, r, g, b);               gfx_rect(x, y, 2, len, z, r, g, b);
    gfx_rect(x + w - len, y, len, 2, z, r, g, b);     gfx_rect(x + w - 2, y, 2, len, z, r, g, b);
    gfx_rect(x, y + h - 2, len, 2, z, r, g, b);       gfx_rect(x, y + h - len, 2, len, z, r, g, b);
    gfx_rect(x + w - len, y + h - 2, len, 2, z, r, g, b); gfx_rect(x + w - 2, y + h - len, 2, len, z, r, g, b);
}

/* The main window of a screen: a glass pane (what is behind shows through,
 * darkened) in a steel-blue frame with lit corners and a glow along the top. */
void gfx_panel(int x, int y, int w, int h, int z) {
    corners(x, y, w, h, z, 22, 150, 230, 255);
    gfx_grad_h(x + 2, y + 2, w / 2 - 2, 2, z, 0x0A1E3C, 0x50B4F0);      /* top light, fading to the corners */
    gfx_grad_h(x + w / 2, y + 2, w / 2 - 2, 2, z, 0x50B4F0, 0x0A1E3C);
    gfx_grad(x, y, w, 2, z, 0x3C6EA0, 0x3C6EA0);                          /* the frame */
    gfx_grad(x, y + h - 2, w, 2, z, 0x1A2E58, 0x1A2E58);
    gfx_grad(x, y, 2, h, z, 0x3C6EA0, 0x1A2E58);
    gfx_grad(x + w - 2, y, 2, h, z, 0x3C6EA0, 0x1A2E58);
    gfx_blend(x + 2, y + 4, w - 4, 56, z, GFX_ADD, 0x0A1C34, 0x000000);  /* sheen under the title */
    /* Tinted glass: what is behind shows through at half strength. (One blend only:
     * a second full-screen pass would cost too much of the frame at 640x480.) */
    gfx_blend(x + 2, y + 2, w - 4, h - 4, z, GFX_HALF, 0x0E1838, 0x03050C);
}

void gfx_card(int x, int y, int w, int h, int z) {
    corners(x, y, w, h, z, 8, 110, 170, 230);
    gfx_grad(x + 1, y + 1, w - 2, h - 2, z, 0x101C40, 0x060A1C);
    gfx_grad(x, y, w, h, z, 0x3A5C94, 0x1C2C54);
}

void gfx_bar(int x, int y, int w, int h, int z, int value, int max, int r, int g, int b) {
    if (max < 1) max = 1;
    if (value < 0) value = 0;
    if (value > max) value = max;
    int fill = (w - 2) * value / max;
    for (int q = 1; q < 4; q++) gfx_rect(x + w * q / 4, y + 1, 1, h - 2, z, 4, 6, 14);   /* quarter marks */
    if (fill > 0) {
        u32 hi = (u32) clamp255(r + 70) << 16 | (u32) clamp255(g + 70) << 8 | (u32) clamp255(b + 70);
        u32 lo = (u32) (r * 5 / 8) << 16 | (u32) (g * 5 / 8) << 8 | (u32) (b * 5 / 8);
        gfx_rect(x + 1, y + 1, fill, 1, z, clamp255(r + 110), clamp255(g + 110), clamp255(b + 110));
        gfx_grad(x + 1, y + 1, fill, h - 2, z, hi, lo);
    }
    gfx_grad(x + 1, y + 1, w - 2, h - 2, z, 0x080C1A, 0x141C34);
    gfx_rect(x, y, w, h, z, 44, 60, 96);
}

void gfx_button(int x, int y, int w, int h, int z, int state, int scale, const char *label) {
    int tr = 214, tg = 224, tb = 240;
    u32 top = 0x1A2C5C, bot = 0x0A1230, edge_t = 0x4C74B0, edge_b = 0x22345E;
    if (state == BTN_SELECTED) { tr = 255; tg = 236; tb = 120; top = 0x4A4A2C; bot = 0x1C2248; edge_t = 0xFFE678; edge_b = 0xB07A20; }
    if (state == BTN_DISABLED) { tr = 104; tg = 112; tb = 132; top = 0x0E1428; bot = 0x080C1C; edge_t = 0x26324E; edge_b = 0x161E36; }
    if (state == BTN_MARKED)   { tr = 130; tg = 226; tb = 255; }
    int th = 8 * scale;
    gfx_text_big(x + (state == BTN_SELECTED ? 14 : 10), y + (h - th) / 2, z, scale, tr, tg, tb, label);
    if (state == BTN_SELECTED) {
        gfx_rect(x + 3, y + 3, 4, h - 6, z, 255, 236, 120);                     /* marker */
        gfx_blend(x + 1, y + 1, w - 2, h / 2, z, GFX_ADD, 0x30280C, 0x000000);   /* light on the top half */
    }
    gfx_grad(x + 1, y + 1, w - 2, h - 2, z, top, bot);
    gfx_grad(x, y, w, h, z, edge_t, edge_b);
}

int gfx_prim_bytes_used(void) {
    RenderBuffer *buffer = &ctx.buffers[ctx.active_buffer];
    return (int)(ctx.next_packet - buffer->buffer);
}

int gfx_prim_overflows(void) { return ctx.overflows; }
