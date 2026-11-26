#include <string.h>
#include <psxpad.h>

#include "audio.h"
#include "input.h"
#include "save.h"

#ifdef FRONTIER_HOST
#include <stdio.h>
#include <stdlib.h>
#include "host_spu.h"
#else
#include <psxapi.h>
#include <psxgpu.h>
#include <psxspu.h>
#include <hwregs_c.h>
#endif

#define BANK_ADDR   0x1010          /* sound RAM below this is the SPU's own */
#define MUSIC_V     10              /* voices 0..9 play music, 10..23 effects */
#define SFX_V       14
#define MUSIC_MASK  ((1u << MUSIC_V) - 1)

/* Overall levels (out of 256), so a full song plus a battle stays under clipping. */
#define MUSIC_GAIN  150
#define SFX_GAIN    230

/* ------------------------------------------------------------------------ */
/* The hardware (or, in the preview tool, its stand-in)                      */
/* ------------------------------------------------------------------------ */

#ifdef FRONTIER_HOST
static void hw_voice(int v, int l, int r, int pitch, u32 addr, u16 a1, u16 a2) { hspu_voice(v, l, r, pitch, addr, a1, a2); }
static void hw_vol(int v, int l, int r) { hspu_vol(v, l, r); }
static void hw_keys(u32 on, u32 off) { if (off) hspu_keyoff(off); if (on) hspu_keyon(on); }
static u32  hw_now(void) { return (u32) hspu_vblanks(); }
#else
static void hw_voice(int v, int l, int r, int pitch, u32 addr, u16 a1, u16 a2) {
    SPU_CH_VOL_L(v) = (u16) l;
    SPU_CH_VOL_R(v) = (u16) r;
    SPU_CH_FREQ(v)  = (u16) pitch;
    SPU_CH_ADDR(v)  = getSPUAddr(addr);
    SPU_CH_ADSR1(v) = a1;
    SPU_CH_ADSR2(v) = a2;
}
static void hw_vol(int v, int l, int r) { SPU_CH_VOL_L(v) = (u16) l; SPU_CH_VOL_R(v) = (u16) r; }
static void hw_keys(u32 on, u32 off) {
    if (off) { SPU_KEY_OFF1 = (u16) off; SPU_KEY_OFF2 = (u16) (off >> 16); }
    if (on)  { SPU_KEY_ON1  = (u16) on;  SPU_KEY_ON2  = (u16) (on >> 16); }
}
static volatile u32 vbl_count;
static u32 hw_now(void) { return vbl_count; }
#endif

/* ------------------------------------------------------------------------ */
/* Settings                                                                  */
/* ------------------------------------------------------------------------ */

static volatile u8 music_vol = 7, sfx_vol = 8;
int  audio_music_volume(void) { return music_vol; }
int  audio_sfx_volume(void)   { return sfx_vol; }
void audio_set_volume(int m, int s) {
    music_vol = (u8) (m < 0 ? 0 : m > 10 ? 10 : m);
    sfx_vol   = (u8) (s < 0 ? 0 : s > 10 ? 10 : s);
}
/* 0..10 to a loudness that sounds even: 0, 1, 4, 9 ... 100. */
static int curve(int v) { return v * v; }

/* ------------------------------------------------------------------------ */
/* Pitch                                                                     */
/* ------------------------------------------------------------------------ */

static const u16 semitone[12] = { 4096, 4340, 4598, 4871, 5161, 5468, 5793, 6137, 6502, 6889, 7298, 7732 };

static int note_pitch(const AudInst *in, int note) {
    int d = note - in->ref, oct = 0;
    while (d < 0)   { d += 12; oct--; }
    while (d >= 12) { d -= 12; oct++; }
    s32 p = ((s32) in->pitch * semitone[d]) >> 12;
    p = oct >= 0 ? p << oct : p >> -oct;
    return p > 0x3FFF ? 0x3FFF : p < 1 ? 1 : (int) p;
}

/* ------------------------------------------------------------------------ */
/* The music sequencer (runs in the vertical blank)                         */
/* ------------------------------------------------------------------------ */

typedef struct {
    const u8 *p, *start;
    u16 wait, left;
    u8  inst, vel, playing;
    s8  pan;
} Chan;

static Chan chans[AUD_SONG_CH];
static int  nch, cur_song = SONG_NONE;
static volatile int req_song = SONG_NONE;
static int  fade;                     /* 0..256 */
static u32  acc;                      /* tick accumulator, 16.16 */

static void chan_levels(const Chan *c, int *l, int *r) {
    s32 v = (s32) aud_inst[c->inst].vol * c->vel;                     /* up to 16129 */
    v = v * curve(music_vol) / 100;
    v = (v * fade) >> 8;
    v = (v * MUSIC_GAIN) >> 8;
    int L = c->pan > 0 ? (int) (v * (64 - c->pan) / 64) : (int) v;
    int R = c->pan < 0 ? (int) (v * (64 + c->pan) / 64) : (int) v;
    *l = L > 0x3FFF ? 0x3FFF : L;
    *r = R > 0x3FFF ? 0x3FFF : R;
}

static void song_start(int s) {
#ifdef FRONTIER_HOST
    if (getenv("FR_AUDIOLOG")) fprintf(stderr, "[audio] vblank %d: song %d\n", hspu_vblanks(), s);
#endif
    cur_song = s;
    nch = 0;
    acc = 0;
    if (s < 0) return;
    const AudSong *sg = &aud_song[s];
    nch = sg->nch;
    for (int i = 0; i < nch; i++) {
        Chan *c = &chans[i];
        memset(c, 0, sizeof *c);
        c->p = c->start = aud_seq + sg->off[i];
        c->vel = 100;
    }
}

static void seq_tick(void) {
    u32 on = 0, off = 0;
    for (int i = 0; i < nch; i++) {
        Chan *c = &chans[i];
        if (c->left && --c->left == 0) { off |= 1u << i; c->playing = 0; }
        int guard = 0;
        while (c->wait == 0 && guard++ < 64) {
            u8 b = *c->p++;
            if (b < 0x80) {
                const AudInst *in = &aud_inst[c->inst];
                int l, r;
                chan_levels(c, &l, &r);
                hw_voice(i, l, r, note_pitch(in, b), BANK_ADDR + in->off, in->adsr1, in->adsr2);
                c->left = *c->p++;
                c->wait = *c->p++;
                c->playing = 1;
                on |= 1u << i;
            } else if (b == 0x80) c->inst = *c->p++;
            else if (b == 0x81) c->vel = *c->p++;
            else if (b == 0x82) c->wait = *c->p++;
            else if (b == 0x83) c->pan = (s8) (*c->p++ - 64);
            else c->p = c->start;                           /* 0xFF: the song loops */
        }
        if (c->wait) c->wait--;
    }
    off &= ~on;
    hw_keys(on, off);
}

static void music_vblank(void) {
    int want = req_song;
    if (want != cur_song) {
        if (cur_song >= 0 && fade > 0) fade -= 6;           /* about 2/3 of a second */
        if (cur_song < 0 || fade <= 0) {
            hw_keys(0, MUSIC_MASK);
            fade = 0;
            song_start(want);
        }
    } else if (cur_song >= 0 && fade < 256) {
        fade += 12;
        if (fade > 256) fade = 256;
    }
    if (cur_song < 0) return;
    acc += ((u32) aud_song[cur_song].bpm << 16) / 300;      /* 12 ticks a beat, 60 vblanks a second */
    while (acc >= 0x10000) { acc -= 0x10000; seq_tick(); }
    for (int i = 0; i < nch; i++) {                         /* volume and fades, every vblank */
        int l, r;
        chan_levels(&chans[i], &l, &r);
        hw_vol(i, l, r);
    }
}

static void vblank(void) {
#ifndef FRONTIER_HOST
    vbl_count++;
#endif
    music_vblank();
}

void audio_music(int song) { req_song = song; }
int  audio_song(void) { return req_song; }

/* ------------------------------------------------------------------------ */
/* Sound effects                                                             */
/* ------------------------------------------------------------------------ */

static u32 sfx_until[SFX_V], sfx_started[SFX_V];
static u8  sfx_prio[SFX_V], sfx_id[SFX_V];

static void sfx_play(int id, int pct, int pan) {
    if (id < 0 || id >= AUD_SFX || !sfx_vol || pct <= 0) return;
    const AudSfx *s = &aud_sfx[id];
    u32 now = hw_now();
    int pick = -1;
    for (int k = 0; k < SFX_V; k++)                         /* the same sound twice at once: no */
        if (sfx_id[k] == id && now < sfx_until[k] && now - sfx_started[k] < 3) return;
    for (int k = 0; k < SFX_V; k++)
        if (now >= sfx_until[k]) { pick = k; break; }
    if (pick < 0) {                                         /* all busy: the least important, oldest */
        for (int k = 0; k < SFX_V; k++)
            if (sfx_prio[k] <= s->prio && (pick < 0 || sfx_prio[k] < sfx_prio[pick] ||
                (sfx_prio[k] == sfx_prio[pick] && sfx_started[k] < sfx_started[pick]))) pick = k;
        if (pick < 0) return;
    }
    s32 v = (s32) s->vol * 127 * curve(sfx_vol) / 100;
    v = v * pct / 100;
    v = (v * SFX_GAIN) >> 8;
    if (v > 0x3FFF) v = 0x3FFF;
    int l = pan > 0 ? (int) (v * (64 - pan) / 64) : (int) v;
    int r = pan < 0 ? (int) (v * (64 + pan) / 64) : (int) v;
    int voice = MUSIC_V + pick;
    hw_voice(voice, l, r, s->pitch, BANK_ADDR + s->off, 0x00FF, 0xC000 | (127 << 6) | 0x20 | 6);
    hw_keys(1u << voice, 0);
    sfx_until[pick] = now + s->frames;
    sfx_started[pick] = now;
    sfx_prio[pick] = s->prio;
    sfx_id[pick] = (u8) id;
#ifdef FRONTIER_HOST
    if (getenv("FR_AUDIOLOG")) fprintf(stderr, "[audio] vblank %d: sfx %d at %d%%\n", (int) now, id, pct);
#endif
}

void audio_sfx(int id) { sfx_play(id, 100, 0); }
void audio_sfx_vol(int id, int pct) { sfx_play(id, pct, 0); }

void audio_sfx_at(int id, int dx, int dy) {
    int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    int d = ax > ay ? ax + ay / 2 : ay + ax / 2;
    int pct = 100 - d * 100 / 1300;                         /* fades out over about two screens */
    if (pct < 6) return;
    int pan = dx * 48 / 520;
    if (pan > 48) pan = 48;
    if (pan < -48) pan = -48;
    sfx_play(id, pct, pan);
}

static int lx, ly;
void audio_listen(int x, int y) { lx = x; ly = y; }
void audio_sfx_world(int id, int x, int y) { audio_sfx_at(id, x - lx, y - ly); }

void audio_menu(void) {
    if (input_pressed(PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT)) audio_sfx(SFX_MENU_MOVE);
    if (input_pressed(PAD_CROSS)) audio_sfx(SFX_MENU_OK);
    else if (input_pressed(PAD_CIRCLE)) audio_sfx(SFX_MENU_BACK);
}

/* ------------------------------------------------------------------------ */
/* Start-up                                                                  */
/* ------------------------------------------------------------------------ */

/* The SPU's "Hall" reverb (psx-spx), and its work area at the top of sound RAM. */
static const u16 hall[32] = {
    0x01A5, 0x0139, 0x6000, 0x5000, 0x4C00, 0xB800, 0xBC00, 0xC000,
    0x6000, 0x5C00, 0x15BA, 0x11BB, 0x14C2, 0x10BD, 0x11BC, 0x0DC1,
    0x11C0, 0x0DC3, 0x0DC0, 0x09C1, 0x0BC4, 0x07C1, 0x0A00, 0x06CD,
    0x09C2, 0x05C1, 0x05C0, 0x041A, 0x0274, 0x013A, 0x8000, 0x8000 };
#define REVERB_SIZE 0xADE0
#define REVERB_VOL  0x2400

void audio_init(void) {
#ifdef FRONTIER_HOST
    hspu_upload(BANK_ADDR, aud_bank, AUD_BANK_BYTES);
    hspu_reverb(MUSIC_MASK, REVERB_VOL);
    hspu_set_vblank(vblank);
#else
    static u32 zero[256];                                   /* 1 KB of silence */
    SpuInit();
    SpuSetTransferMode(SPU_TRANSFER_BY_DMA);
    SpuSetTransferStartAddr(BANK_ADDR);
    SpuWrite(aud_bank, (AUD_BANK_BYTES + 63) & ~63);
    SpuIsTransferCompleted(SPU_TRANSFER_WAIT);

    SPU_CTRL &= ~0x0080;                                    /* reverb off while it is set up */
    SPU_REVERB_ADDR = (0x80000 - REVERB_SIZE) / 8;
    for (int i = 0; i < 32; i++) _MMIO16((IOBASE | 0x1dc0) + 2 * i) = hall[i];
    for (u32 a = 0x80000 - REVERB_SIZE; a < 0x80000; a += sizeof zero) {
        SpuSetTransferStartAddr(a);
        SpuWrite(zero, sizeof zero);
        SpuIsTransferCompleted(SPU_TRANSFER_WAIT);
    }
    SPU_REVERB_VOL_L = REVERB_VOL;
    SPU_REVERB_VOL_R = REVERB_VOL;
    SPU_REVERB_ON1 = (u16) MUSIC_MASK;
    SPU_REVERB_ON2 = 0;
    SPU_MASTER_VOL_L = 0x3FFF;
    SPU_MASTER_VOL_R = 0x3FFF;
    SPU_CTRL |= 0x0080;
    VSyncCallback(vblank);
#endif
}

u8 *audio_ser(u8 *p, int load) {
    if (!load || ser_version >= 13) {
        u8 m = music_vol, s = sfx_vol;
        SER(p, load, m); SER(p, load, s);
        if (load) audio_set_volume(m, s);
    }
    return p;
}
