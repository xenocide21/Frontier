/*
 * FRONTIER - low-poly 3D ships (rewritten in Milestone 14e to use the GTE). See model.h.
 */
#include <psxgte.h>
#include <inline_c.h>

#include "gfx.h"
#include "model.h"

#define MAX_V 1002
#define MAX_F 1000
#define BUCKETS 64

static u32 sxy[MAX_V];                    /* screen x (low half) and y, as the GTE stores them */
static int szv[MAX_V];                    /* depth */
static s16 order[MAX_F], next_in[MAX_F], bucket_head[BUCKETS];

void model_init(void) { InitGeom(); }

/* out = a * b, 3x3, FP12. */
static void mul3(s32 out[3][3], s32 a[3][3], s32 b[3][3]) {
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            out[i][j] = (a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j]) >> 12;
}

/* Draws with rotation r (model axes -> camera axes x right, y DOWN, z in),
 * translation t, and light l (camera space, FP12, pointing at the light). */
static void draw(int mesh, s32 r[3][3], s32 tx, s32 ty, s32 tz, int scale, int cx, int cy, int h, int z, int shade) {
    const Mesh *m = &ship_meshes[mesh];
    if (m->nv > MAX_V || m->nf > MAX_F || tz < 64) return;

    MATRIX gm;
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) gm.m[i][j] = (s16) r[i][j];
    gm.t[0] = tx; gm.t[1] = ty; gm.t[2] = tz;
    gte_SetRotMatrix(&gm);
    gte_SetTransMatrix(&gm);
    gte_SetGeomOffset(cx, cy);
    gte_SetGeomScreen(h);

    const SVECTOR *v = (const SVECTOR *) m->v;
    for (int i = 0; i < m->nv; i += 3) {
        gte_ldv3(&v[i], &v[i + 1], &v[i + 2]);
        gte_rtpt();
        gte_stsxy3(&sxy[i], &sxy[i + 1], &sxy[i + 2]);
        gte_stsz3(&szv[i], &szv[i + 1], &szv[i + 2]);
    }

    /* The light in model space: from above, a little left, toward the viewer. */
    static const s32 lc[3] = { -1300, -3300, -2000 };      /* camera space (y down), about unit length */
    s32 lm[3];
    for (int j = 0; j < 3; j++) lm[j] = ((r[0][j] * lc[0] + r[1][j] * lc[1] + r[2][j] * lc[2]) >> 12) * 4096 / scale;

    /* Faces that face us, bucketed by depth. */
    int reach = 210 * scale >> 12;
    for (int b = 0; b < BUCKETS; b++) bucket_head[b] = -1;
    const u16 *f = m->f;
    for (int i = 0; i < m->nf; i++, f += 3) {
        int a = f[0], b = f[1], c = f[2];
        int x0 = (s16) sxy[a], y0 = (s16) (sxy[a] >> 16);
        int cross = ((s16) sxy[b] - x0) * ((s16) (sxy[c] >> 16) - y0) - ((s16) (sxy[b] >> 16) - y0) * ((s16) sxy[c] - x0);
        if (cross >= 0) continue;
        if (szv[a] < 32 || szv[b] < 32 || szv[c] < 32) continue;      /* too near the camera plane */
        int d = (szv[a] + szv[b] + szv[c]) / 3 - tz + reach;
        int k = d * BUCKETS / (2 * reach + 1);
        if (k < 0) k = 0;
        if (k >= BUCKETS) k = BUCKETS - 1;
        next_in[i] = bucket_head[k];
        bucket_head[k] = (s16) i;
    }
    int n = 0;
    for (int k = 0; k < BUCKETS; k++)                    /* nearest first: the last added draws first */
        for (int i = bucket_head[k]; i >= 0; i = next_in[i]) order[n++] = (s16) i;

    for (int j = 0; j < n; j++) {
        int i = order[j];
        const u16 *ff = m->f + i * 3;
        const u8 *col = m->c + i * 4;
        const s8 *nn = m->n + i * 4;
        int lit = 256;
        if (!col[3]) {
            int dot = (nn[0] * lm[0] + nn[1] * lm[1] + nn[2] * lm[2]) >> 7;     /* FP12 */
            if (dot < 0) dot = -dot >> 2;                 /* a faint fill from the far side */
            lit = 96 + (dot * 200 >> 12);                 /* ambient + diffuse, 256 = full */
        }
        lit = lit * shade >> 8;
        int cr = col[0] * lit >> 8, cg = col[1] * lit >> 8, cb = col[2] * lit >> 8;
        if (cr > 255) cr = 255;
        if (cg > 255) cg = 255;
        if (cb > 255) cb = 255;
        u32 a = sxy[ff[0]], b = sxy[ff[1]], c = sxy[ff[2]];
        gfx_tri((s16) a, (s16) (a >> 16), (s16) b, (s16) (b >> 16), (s16) c, (s16) (c >> 16), z, cr, cg, cb);
    }
}

void model_draw(int mesh, int cx, int cy, int radius, int yaw, int pitch, int z) {
    s32 cyw = icos(yaw), syw = isin(yaw), cp = icos(pitch), sp = isin(pitch);
    /* Yaw about the model's up axis, then tilt the top toward the viewer. Rows: screen x, y (down), depth. */
    s32 r[3][3] = { { cyw, syw, 0 },
                    { (syw * sp) >> 12, -(cyw * sp) >> 12, -cp },
                    { -(syw * cp) >> 12, (cyw * cp) >> 12, -sp } };
    draw(mesh, r, 0, 0, 512, 4096, cx, cy, 512 * radius / 118, z, 256);
}

void model_draw_at(int mesh, const ModelPose *p, int cx, int cy, int h, int z, int shade) {
    s32 ch = icos(p->heading), sh = isin(p->heading), cb = icos(p->bank), sb = isin(p->bank);
    s32 cc = icos(p->climb), sc = isin(p->climb), s = p->scale;
    /* Model (x right, y nose, z up) -> world (x right, y away, z up): bank about the nose,
     * climb about the wing axis, then heading about up. */
    s32 bank[3][3]  = { { cb, 0, sb }, { 0, 4096, 0 }, { -sb, 0, cb } };
    s32 climb[3][3] = { { 4096, 0, 0 }, { 0, cc, -sc }, { 0, sc, cc } };
    s32 head[3][3]  = { { ch, sh, 0 }, { -sh, ch, 0 }, { 0, 0, 4096 } };
    /* World -> camera rows (x, y down, depth), with the scale folded in. */
    s32 view[3][3]  = { { s, 0, 0 }, { 0, 0, -s }, { 0, s, 0 } };
    s32 t1[3][3], t2[3][3], r[3][3];
    mul3(t1, climb, bank);
    mul3(t2, head, t1);
    mul3(r, view, t2);
    draw(mesh, r, p->x, -p->y, p->z, s, cx, cy, h, z, shade);
}

int model_project(s32 x, s32 y, s32 z, int cx, int cy, int h, int *sx, int *sy) {
    if (z < 32) return 0;
    *sx = cx + x * h / z;
    *sy = cy - y * h / z;
    return 1;
}
