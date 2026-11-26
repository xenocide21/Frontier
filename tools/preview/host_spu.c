/*
 * Host preview: a software SPU, close enough to hear what the game plays.
 *
 * 24 voices decode SPU ADPCM from a 512 KB sound RAM, step through it at the
 * voice's pitch (4.12, 0x1000 = 44100 Hz), shape it with the SPU's ADSR envelope
 * (psx-spx), and mix to stereo. The reverb is an ordinary Schroeder reverb, not
 * the SPU's, but it sits in the same place. FR_AUDIO=out.wav records it all.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "host_spu.h"

static uint8_t ram[0x80000];
static const int filt[5][2] = { { 0, 0 }, { 60, 0 }, { 115, -52 }, { 98, -55 }, { 122, -60 } };

enum { OFF = 0, ATT, DEC, SUS, REL };

typedef struct {
    int voll, volr, pitch;
    uint32_t start, cur, loop;
    uint16_t a1, a2;
    int16_t buf[28];
    int idx, h1, h2, prev;
    uint32_t frac;
    int phase, level, ecount;
    uint8_t flags;
} Voice;

static Voice vc[24];
static uint32_t rev_mask;
static int rev_vol;
static void (*vb_cb)(void);
static int vbl;

static int16_t *rec;
static size_t rec_n, rec_cap;
static const char *rec_path;

static void write_wav(void) {
    if (!rec_path || !rec_n) return;
    FILE *f = fopen(rec_path, "wb");
    if (!f) return;
    uint32_t data = (uint32_t) (rec_n * 2), riff = 36 + data, sr = 44100, br = sr * 4, fmt = 16;
    uint16_t pcm = 1, ch = 2, ba = 4, bits = 16;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    fwrite(&fmt, 4, 1, f); fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f);
    fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
    fwrite(rec, 2, rec_n, f);
    fclose(f);
}

void hspu_upload(uint32_t addr, const void *data, uint32_t size) {
    if (addr + size > sizeof ram) size = sizeof ram - addr;
    memcpy(ram + addr, data, size);
    if (!rec_path && getenv("FR_AUDIO")) { rec_path = getenv("FR_AUDIO"); atexit(write_wav); }
}

static void decode(Voice *v) {
    const uint8_t *b = ram + (v->cur & 0x7FFF0);
    int sh = b[0] & 15, f = b[0] >> 4;
    if (f > 4) f = 4;
    if (sh > 12) sh = 9;
    v->flags = b[1];
    if (v->flags & 4) v->loop = v->cur;
    for (int i = 0; i < 28; i++) {
        int n = (b[2 + i / 2] >> ((i & 1) * 4)) & 15;
        if (n >= 8) n -= 16;
        int s = ((n << 12) >> sh) + ((v->h1 * filt[f][0] + v->h2 * filt[f][1] + 32) >> 6);
        if (s > 32767) s = 32767;
        if (s < -32768) s = -32768;
        v->buf[i] = (int16_t) s;
        v->h2 = v->h1; v->h1 = s;
    }
}

static void next_block(Voice *v) {
    if (v->flags & 1) {
        v->cur = v->loop;
        if (!(v->flags & 2)) { v->phase = OFF; v->level = 0; }
    } else v->cur += 16;
    decode(v);
}

void hspu_voice(int n, int l, int r, int pitch, uint32_t addr, uint16_t a1, uint16_t a2) {
    Voice *v = &vc[n];
    v->voll = l; v->volr = r; v->pitch = pitch; v->start = addr; v->a1 = a1; v->a2 = a2;
}
void hspu_vol(int n, int l, int r) { vc[n].voll = l; vc[n].volr = r; }

void hspu_keyon(uint32_t mask) {
    for (int n = 0; n < 24; n++) if (mask & (1u << n)) {
        Voice *v = &vc[n];
        v->cur = v->loop = v->start;
        v->h1 = v->h2 = 0; v->idx = 0; v->frac = 0; v->prev = 0;
        v->phase = ATT; v->level = 0; v->ecount = 0;
        decode(v);
    }
}
void hspu_keyoff(uint32_t mask) {
    for (int n = 0; n < 24; n++) if ((mask & (1u << n)) && vc[n].phase != OFF) vc[n].phase = REL;
}
void hspu_reverb(uint32_t mask, int vol) { rev_mask = mask; rev_vol = vol; }
void hspu_set_vblank(void (*f)(void)) { vb_cb = f; }
int  hspu_vblanks(void) { return vbl; }

/* One envelope step (psx-spx). */
static void env(Voice *v) {
    int rate, up, ex;
    switch (v->phase) {
    case ATT: rate = (v->a1 >> 8) & 0x7F; up = 1; ex = v->a1 >> 15; break;
    case DEC: rate = ((v->a1 >> 4) & 0xF) * 4; up = 0; ex = 1; break;
    case SUS: rate = (v->a2 >> 6) & 0x7F; up = !((v->a2 >> 14) & 1); ex = v->a2 >> 15; break;
    case REL: rate = (v->a2 & 0x1F) * 4; up = 0; ex = (v->a2 >> 5) & 1; break;
    default: return;
    }
    int shift = rate >> 2;
    int step = up ? 7 - (rate & 3) : -8 + (rate & 3);
    int cycles = 1 << (shift > 11 ? shift - 11 : 0);
    step <<= (shift < 11 ? 11 - shift : 0);
    if (up && ex && v->level > 0x6000) cycles *= 4;
    if (++v->ecount < cycles) return;
    v->ecount = 0;
    if (!up && ex) step = (int) (((int64_t) step * v->level) >> 15);
    v->level += step;
    if (v->level > 0x7FFF) v->level = 0x7FFF;
    if (v->level < 0) v->level = 0;
    int sl = ((v->a1 & 15) + 1) * 0x800;
    if (v->phase == ATT && v->level >= 0x7FFF) v->phase = DEC;
    else if (v->phase == DEC && v->level <= sl) v->phase = SUS;
    else if (v->phase == REL && v->level <= 0) v->phase = OFF;
}

/* A plain stereo reverb. */
#define NC 4
static float comb_buf[2][NC][1800], comb_lp[2][NC], ap_buf[2][2][600];
static int comb_pos[2][NC], ap_pos[2][2];
static const int comb_len[NC] = { 1557, 1617, 1491, 1422 }, ap_len[2] = { 556, 441 };

static float reverb(int side, float in) {
    float out = 0;
    for (int c = 0; c < NC; c++) {
        int len = comb_len[c] + side * 23;
        float *b = comb_buf[side][c];
        float y = b[comb_pos[side][c]];
        comb_lp[side][c] = y * 0.75f + comb_lp[side][c] * 0.25f;
        b[comb_pos[side][c]] = in + comb_lp[side][c] * 0.86f;
        comb_pos[side][c] = (comb_pos[side][c] + 1) % len;
        out += y;
    }
    for (int a = 0; a < 2; a++) {
        int len = ap_len[a] + side * 13;
        float *b = ap_buf[side][a];
        float bo = b[ap_pos[side][a]];
        b[ap_pos[side][a]] = out + bo * 0.5f;
        ap_pos[side][a] = (ap_pos[side][a] + 1) % len;
        out = bo - out;
    }
    return out;
}

static void mix(int n) {
    if (!rec_path) return;
    if (rec_n + (size_t) n * 2 > rec_cap) {
        rec_cap = (rec_n + n * 2) * 2 + 44100 * 2;
        rec = realloc(rec, rec_cap * 2);
    }
    for (int i = 0; i < n; i++) {
        float L = 0, R = 0, rin = 0;
        for (int k = 0; k < 24; k++) {
            Voice *v = &vc[k];
            if (v->phase == OFF) continue;
            int s = v->buf[v->idx];
            float x = (float) (v->prev + ((s - v->prev) * (int) (v->frac & 0xFFF) >> 12));
            x = x * v->level / 32768.0f;
            float l = x * v->voll / 16384.0f, r = x * v->volr / 16384.0f;
            L += l; R += r;
            if (rev_mask & (1u << k)) rin += (l + r) * 0.5f;
            env(v);
            v->frac += (uint32_t) (v->pitch > 0x3FFF ? 0x3FFF : v->pitch);
            while (v->frac >= 0x1000) {
                v->frac -= 0x1000;
                v->prev = v->buf[v->idx];
                if (++v->idx >= 28) { v->idx = 0; next_block(v); }
            }
        }
        float w = rev_vol / 32768.0f * 0.22f;
        L += reverb(0, rin) * w;
        R += reverb(1, rin) * w;
        int li = (int) L, ri = (int) R;
        rec[rec_n++] = (int16_t) (li > 32767 ? 32767 : li < -32768 ? -32768 : li);
        rec[rec_n++] = (int16_t) (ri > 32767 ? 32767 : ri < -32768 ? -32768 : ri);
    }
}

void hspu_vblank_tick(void) {
    vbl++;
    if (vb_cb) vb_cb();
    mix(735);
}
