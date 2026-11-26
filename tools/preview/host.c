/*
 * FRONTIER preview renderer (Milestone 14e): runs the real game code on a PC
 * and draws its PlayStation display lists with a small software model of the
 * GPU (flat / Gouraud / textured polygons, sprites, lines, 4-bit textures with
 * palettes, the four blend modes, dithering, the mask bit) and of the GTE
 * operations model.c uses. Not an emulator: no CPU, no timing, no sound.
 * It exists so screens can be looked at before a disc is burned.
 *
 *   FR_SCRIPT="40:START 90:CROSS 120-200:UP"   button presses by frame
 *   FR_SHOTS="30,100,250"                      frames to save as PPM, then exit
 *   FR_OUT=/tmp/prev                           where
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <psxgpu.h>
#include <psxgte.h>
#include <psxpad.h>
#include "common.h"
#include "input.h"
#include "save.h"
#include "host_spu.h"
#include "gfx.h"

static uint16_t vram[512][1024];
static int g_abr, g_tpx, g_tpy, g_dither, frame_no, vbl;

/* ---- display lists ---- */
#define MAXP 20000
static void *plist[OT_LENGTH][MAXP]; static int pn[OT_LENGTH];
void host_add_prim(int z, void *p) { if (pn[z] < MAXP) plist[z][pn[z]++] = p; }

static const int dmat[4][4] = { {-4, 0, -3, 1}, {2, -2, 3, -1}, {-3, 1, -4, 0}, {3, -1, 2, -2} };

static void put(int x, int y, int r, int g, int b, int semi, int dith) {
    if (x < 0 || y < 0 || x >= 640 || y >= 480) return;
    if (dith && g_dither) { int d = dmat[y & 3][x & 3]; r += d; g += d; b += d; }
    r = r < 0 ? 0 : r > 255 ? 255 : r; g = g < 0 ? 0 : g > 255 ? 255 : g; b = b < 0 ? 0 : b > 255 ? 255 : b;
    r >>= 3; g >>= 3; b >>= 3;
    if (semi) {
        uint16_t d = vram[y][x]; int br = d & 31, bg = d >> 5 & 31, bb = d >> 10 & 31;
        switch (g_abr) {
        case 0: r = (br + r) / 2; g = (bg + g) / 2; b = (bb + b) / 2; break;
        case 1: r += br; g += bg; b += bb; break;
        case 2: r = br - r; g = bg - g; b = bb - b; break;
        default: r = br + r / 4; g = bg + g / 4; b = bb + b / 4; break;
        }
        r = r < 0 ? 0 : r > 31 ? 31 : r; g = g < 0 ? 0 : g > 31 ? 31 : g; b = b < 0 ? 0 : b > 31 ? 31 : b;
    }
    vram[y][x] = (uint16_t) (r | g << 5 | b << 10);
}

static uint16_t texel(int u, int v, int clut) {
    u &= 255; v &= 255;
    int nib = (vram[(g_tpy + v) & 511][(g_tpx + (u >> 2)) & 1023] >> ((u & 3) * 4)) & 15;
    return vram[(clut >> 6) & 511][((clut & 63) * 16 + nib) & 1023];
}

typedef struct { int x, y, u, v, r, g, b; } V;

static void shade(int x, int y, double r, double g, double b, double u, double v, int tex, int raw, int semi, int goura, int clut) {
    if (tex) {
        uint16_t t = texel((int) floor(u + 1.0 / 512), (int) floor(v + 1.0 / 512), clut);
        if (!t) return;
        int tr = (t & 31) << 3, tg = (t >> 5 & 31) << 3, tb = (t >> 10 & 31) << 3;
        if (!raw) { tr = (int) (tr * r / 128); tg = (int) (tg * g / 128); tb = (int) (tb * b / 128); }
        put(x, y, tr, tg, tb, semi && (t & 0x8000), !raw);
    } else put(x, y, (int) r, (int) g, (int) b, semi, goura);
}

static long edge(const V *a, const V *b, int x, int y) { return (long) (b->x - a->x) * (y - a->y) - (long) (b->y - a->y) * (x - a->x); }
static int topleft(const V *a, const V *b) { int dx = b->x - a->x, dy = b->y - a->y; return dy < 0 || (dy == 0 && dx > 0); }

static void tri(V a, V b, V c, int tex, int raw, int semi, int goura, int clut) {
    long area = edge(&a, &b, c.x, c.y);
    if (area == 0) return;
    if (area < 0) { V t = b; b = c; c = t; area = -area; }
    int x0 = a.x < b.x ? a.x : b.x, x1 = a.x > b.x ? a.x : b.x, y0 = a.y < b.y ? a.y : b.y, y1 = a.y > b.y ? a.y : b.y;
    if (c.x < x0) x0 = c.x; if (c.x > x1) x1 = c.x; if (c.y < y0) y0 = c.y; if (c.y > y1) y1 = c.y;
    if (x1 - x0 > 1023 || y1 - y0 > 511) return;
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > 639) x1 = 639; if (y1 > 479) y1 = 479;
    int b0 = topleft(&b, &c) ? 0 : 1, b1 = topleft(&c, &a) ? 0 : 1, b2 = topleft(&a, &b) ? 0 : 1;
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
        long w0 = edge(&b, &c, x, y), w1 = edge(&c, &a, x, y), w2 = edge(&a, &b, x, y);
        if (w0 < b0 || w1 < b1 || w2 < b2) continue;
        double l0 = (double) w0 / area, l1 = (double) w1 / area, l2 = (double) w2 / area;
        shade(x, y, a.r * l0 + b.r * l1 + c.r * l2, a.g * l0 + b.g * l1 + c.g * l2, a.b * l0 + b.b * l1 + c.b * l2,
              a.u * l0 + b.u * l1 + c.u * l2, a.v * l0 + b.v * l1 + c.v * l2, tex, raw, semi, goura, clut);
    }
}

static void line(V a, V b, int semi, int goura) {
    int dx = abs(b.x - a.x), dy = abs(b.y - a.y), n = dx > dy ? dx : dy;
    if (n > 1024) return;
    for (int i = 0; i <= n; i++) {
        double t = n ? (double) i / n : 0;
        put((int) lround(a.x + (b.x - a.x) * t), (int) lround(a.y + (b.y - a.y) * t),
            (int) (a.r + (b.r - a.r) * t), (int) (a.g + (b.g - a.g) * t), (int) (a.b + (b.b - a.b) * t), semi, goura);
    }
}

static void set_tpage(int tp) { g_tpx = (tp & 15) * 64; g_tpy = (tp >> 4 & 1) * 256; g_abr = tp >> 5 & 3; }

static void exec(const uint32_t *w) {
    int cmd = w[0] >> 24;
    if (cmd == 0xE1) { set_tpage(w[0] & 0xFFFF); g_dither = w[0] >> 9 & 1; return; }
    if (cmd >= 0x20 && cmd < 0x40) {
        int n = cmd & 8 ? 4 : 3, goura = cmd & 0x10, tex = cmd & 4, semi = cmd & 2, raw = cmd & 1, clut = 0, i = 1;
        V v[4]; uint32_t col = w[0];
        for (int k = 0; k < n; k++) {
            if (k > 0 && goura) col = w[i++];
            v[k].r = col & 255; v[k].g = col >> 8 & 255; v[k].b = col >> 16 & 255;
            v[k].x = (int16_t) (w[i] & 0xFFFF); v[k].y = (int16_t) (w[i] >> 16); i++;
            v[k].u = v[k].v = 0;
            if (tex) { v[k].u = w[i] & 255; v[k].v = w[i] >> 8 & 255; if (k == 0) clut = w[i] >> 16; if (k == 1) set_tpage(w[i] >> 16); i++; }
        }
        if (tex) { /* the page word comes with vertex 1 but applies to the whole polygon */ }
        tri(v[0], v[1], v[2], tex, raw, semi, goura, clut);
        if (n == 4) tri(v[1], v[2], v[3], tex, raw, semi, goura, clut);
        return;
    }
    if (cmd >= 0x40 && cmd < 0x60) {
        int goura = cmd & 0x10, semi = cmd & 2, i = 1; V a, b; uint32_t col = w[0];
        a.r = col & 255; a.g = col >> 8 & 255; a.b = col >> 16 & 255; a.x = (int16_t) (w[i] & 0xFFFF); a.y = (int16_t) (w[i] >> 16); i++;
        if (goura) col = w[i++];
        b.r = col & 255; b.g = col >> 8 & 255; b.b = col >> 16 & 255; b.x = (int16_t) (w[i] & 0xFFFF); b.y = (int16_t) (w[i] >> 16);
        line(a, b, semi, goura);
        return;
    }
    if (cmd >= 0x60 && cmd < 0x80) {
        int sz = cmd >> 3 & 3, tex = cmd & 4, semi = cmd & 2, raw = cmd & 1, i = 1, clut = 0, u0 = 0, v0 = 0;
        int r = w[0] & 255, g = w[0] >> 8 & 255, b = w[0] >> 16 & 255;
        int x = (int16_t) (w[i] & 0xFFFF), y = (int16_t) (w[i] >> 16); i++;
        if (tex) { u0 = w[i] & 255; v0 = w[i] >> 8 & 255; clut = w[i] >> 16; i++; }
        int ww = sz == 0 ? (int) (w[i] & 0xFFFF) : sz == 1 ? 1 : sz == 2 ? 8 : 16, hh = sz == 0 ? (int) (w[i] >> 16) : ww;
        if (ww > 1023 || hh > 511) return;
        for (int yy = 0; yy < hh; yy++) for (int xx = 0; xx < ww; xx++)
            shade(x + xx, y + yy, r, g, b, u0 + xx, v0 + yy, tex, raw, semi, 0, clut);
        return;
    }
}

static const char *shots, *outdir;
static int next_shot(void) { return shots && *shots ? atoi(shots) : -1; }

void host_present(int dither) {
    g_dither = dither; g_abr = 0;
    for (int z = OT_LENGTH - 1; z >= 0; z--) {
        for (int i = pn[z] - 1; i >= 0; i--) exec((const uint32_t *) plist[z][i] + 1);
        pn[z] = 0;
    }
    if (frame_no == next_shot()) {
        char fn[256]; snprintf(fn, sizeof fn, "%s/shot_%05d.ppm", outdir, frame_no);
        FILE *f = fopen(fn, "wb"); fprintf(f, "P6 640 480 255\n");
        for (int y = 0; y < 480; y++) for (int x = 0; x < 640; x++) {
            uint16_t p = vram[y][x]; unsigned char c[3] = { (p & 31) * 255 / 31, (p >> 5 & 31) * 255 / 31, (p >> 10 & 31) * 255 / 31 };
            fwrite(c, 1, 3, f);
        }
        fclose(f);
        while (*shots && *shots != ',') shots++;
        if (*shots == ',') shots++;
        if (!*shots) exit(0);
    }
    frame_no++;
    if (frame_no > 200000) exit(1);
}

/* ---- SDK stubs ---- */
void ResetGraph(int m) { (void) m; shots = getenv("FR_SHOTS"); outdir = getenv("FR_OUT") ? getenv("FR_OUT") : "/tmp"; }
void SetDispMask(int m) { (void) m; }
void PutDispEnv(const DISPENV *e) { (void) e; }
DISPENV *SetDefDispEnv(DISPENV *e, int x, int y, int w, int h) { (void) x; (void) y; (void) w; (void) h; memset(e, 0, sizeof *e); return e; }
DRAWENV *SetDefDrawEnv(DRAWENV *e, int x, int y, int w, int h) { (void) x; (void) y; (void) w; (void) h; memset(e, 0, sizeof *e); return e; }
int VSync(int mode) { if (mode < 0) return vbl; if (mode == 1) return 100; hspu_vblank_tick(); return ++vbl; }
int DrawSync(int m) { (void) m; return 0; }
void ClearOTagR(uint32_t *ot, size_t n) { (void) ot; (void) n; }
int LoadImage(const RECT *r, const uint32_t *d) {
    const uint16_t *s = (const uint16_t *) d;
    for (int y = 0; y < r->h; y++) for (int x = 0; x < r->w; x++) vram[(r->y + y) & 511][(r->x + x) & 1023] = *s++;
    return 0;
}
int isin(int a) { return (int) lround(sin(a * 2 * M_PI / 4096) * 4096); }
int icos(int a) { return (int) lround(cos(a * 2 * M_PI / 4096) * 4096); }
void InitGeom(void) {}

/* ---- GTE ---- */
static MATRIX gm; static int ofx, ofy, gh; static SVECTOR gv[3]; static int sx[3], sy[3], sz[3], mac0;
void h_SetRotMatrix(const MATRIX *m) { memcpy(gm.m, m->m, sizeof gm.m); }
void h_SetTransMatrix(const MATRIX *m) { memcpy(gm.t, m->t, sizeof gm.t); }
void h_SetGeomOffset(int x, int y) { ofx = x; ofy = y; }
void h_SetGeomScreen(int h) { gh = h; }
static void push(const SVECTOR *v) {
    long long x = ((long long) gm.t[0] * 4096 + gm.m[0][0] * v->vx + gm.m[0][1] * v->vy + gm.m[0][2] * v->vz) >> 12;
    long long y = ((long long) gm.t[1] * 4096 + gm.m[1][0] * v->vx + gm.m[1][1] * v->vy + gm.m[1][2] * v->vz) >> 12;
    long long z = ((long long) gm.t[2] * 4096 + gm.m[2][0] * v->vx + gm.m[2][1] * v->vy + gm.m[2][2] * v->vz) >> 12;
    if (z < 0) z = 0; if (z > 65535) z = 65535;
    long long q = z > gh / 2 ? (((long long) gh << 16) + z / 2) / z : 0x1FFFF;
    if (q > 0x1FFFF) q = 0x1FFFF;
    sx[0] = sx[1]; sy[0] = sy[1]; sz[0] = sz[1]; sx[1] = sx[2]; sy[1] = sy[2]; sz[1] = sz[2];
    long long px = ofx + ((x * q) >> 16), py = ofy + ((y * q) >> 16);
    sx[2] = (int) (px < -1024 ? -1024 : px > 1023 ? 1023 : px); sy[2] = (int) (py < -1024 ? -1024 : py > 1023 ? 1023 : py); sz[2] = (int) z;
}
void h_ldv3(const SVECTOR *a, const SVECTOR *b, const SVECTOR *c) { gv[0] = *a; gv[1] = *b; gv[2] = *c; }
void h_ldv0(const SVECTOR *a) { gv[0] = *a; }
void h_rtpt(void) { push(&gv[0]); push(&gv[1]); push(&gv[2]); }
void h_rtps(void) { push(&gv[0]); }
void h_stsxy3(void *a, void *b, void *c) { ((int16_t *) a)[0] = sx[0]; ((int16_t *) a)[1] = sy[0]; ((int16_t *) b)[0] = sx[1]; ((int16_t *) b)[1] = sy[1]; ((int16_t *) c)[0] = sx[2]; ((int16_t *) c)[1] = sy[2]; }
void h_stsxy(void *a) { ((int16_t *) a)[0] = sx[2]; ((int16_t *) a)[1] = sy[2]; }
void h_stsz3(int *a, int *b, int *c) { *a = sz[0]; *b = sz[1]; *c = sz[2]; }
void h_stsz(int *a) { *a = sz[2]; }
void h_nclip(void) { mac0 = sx[0] * sy[1] + sx[1] * sy[2] + sx[2] * sy[0] - sx[0] * sy[2] - sx[1] * sy[0] - sx[2] * sy[1]; }
void h_stopz(int *p) { *p = mac0; }

/* ---- input: a script of button presses ---- */
static u16 held, pressed;
static u16 button(const char *n, int len) {
    static const struct { const char *n; u16 b; } t[] = { {"UP", PAD_UP}, {"DOWN", PAD_DOWN}, {"LEFT", PAD_LEFT}, {"RIGHT", PAD_RIGHT},
        {"CROSS", PAD_CROSS}, {"CIRCLE", PAD_CIRCLE}, {"SQUARE", PAD_SQUARE}, {"TRIANGLE", PAD_TRIANGLE}, {"START", PAD_START},
        {"SELECT", PAD_SELECT}, {"L1", PAD_L1}, {"R1", PAD_R1}, {"L2", PAD_L2}, {"R2", PAD_R2} };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) if ((int) strlen(t[i].n) == len && !strncmp(t[i].n, n, len)) return t[i].b;
    return 0;
}
void input_init(void) {}
void input_update(void) {
    u16 now = 0; const char *s = getenv("FR_SCRIPT");
    while (s && *s) {
        int a = atoi(s), b = a; const char *p = s;
        while (*p && *p != ':' && *p != '-') p++;
        if (*p == '-') { b = atoi(p + 1); while (*p && *p != ':') p++; }
        if (*p == ':') p++;
        const char *e = p; while (*e && *e != ' ') e++;
        if (frame_no >= a && frame_no <= b) now |= button(p, (int) (e - p));
        s = *e ? e + 1 : e;
    }
    pressed = now & ~held; held = now;
}
int input_connected(void) { return 1; }
int input_held(u16 b) { return (held & b) != 0; }
int input_pressed(u16 b) { return (pressed & b) != 0; }
int input_analog(void) { return 0; }
int input_stick(int s, int a) { (void) s; (void) a; return 0; }

/* ---- the memory card: a folder ---- */
#include <sys/stat.h>
static FILE *card_files[8];
static void card_path(char *out, int len, const char *name) {
    const char *dir = getenv("FR_CARD") ? getenv("FR_CARD") : "/tmp/frontier_card";
    mkdir(dir, 0755);
    snprintf(out, len, "%s/%s", dir, name + 5);          /* drop "bu00:" */
}
int host_card_open(const char *name, int mode) {
    char p[256]; card_path(p, sizeof p, name);
    FILE *f = NULL;
    if (mode & 0x0200) { f = fopen(p, "wb"); if (f) { fclose(f); f = NULL; } return 7; }   /* FCREATE */
    if (mode & 0x0002) { FILE *t = fopen(p, "rb"); if (!t) return -1; fclose(t); f = fopen(p, "r+b"); }
    else f = fopen(p, "rb");
    if (!f) return -1;
    for (int i = 0; i < 8; i++) if (!card_files[i]) { card_files[i] = f; return i; }
    fclose(f); return -1;
}
int host_card_read(int fd, void *b, int n) { return fd >= 0 && fd < 8 && card_files[fd] ? (int) fread(b, 1, n, card_files[fd]) : -1; }
int host_card_write(int fd, const void *b, int n) { return fd >= 0 && fd < 8 && card_files[fd] ? (int) fwrite(b, 1, n, card_files[fd]) : -1; }
void host_card_close(int fd) { if (fd >= 0 && fd < 8 && card_files[fd]) { fclose(card_files[fd]); card_files[fd] = NULL; } }
struct DIRENTRY *host_card_first(const char *name, struct DIRENTRY *e) {
    char p[256]; card_path(p, sizeof p, name);
    FILE *f = fopen(p, "rb"); if (!f) return NULL; fclose(f); return e;
}
