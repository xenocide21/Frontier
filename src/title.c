/*
 * FRONTIER - the title screen, pilot select and opening briefing (Milestone 14a).
 * Placeholder presentation: the logo is text over drifting stars. The intro
 * movie and real title art come later.
 */
#include <stdio.h>
#include <psxpad.h>

#include "audio.h"
#include "galaxy.h"
#include "gfx.h"
#include "input.h"
#include "save.h"
#include "ship.h"
#include "title.h"
#include "sysmenu.h"
#include "model.h"
#include "rng.h"
#include "world.h"

const Pilot pilots[PILOT_COUNT] = {
    { "CHAD BEEFSTEEL", "CHAD BEEFSTEEL", DEF_WAYFARER, 0, 10, 0, 3000, 0,
      "THE DEFAULT. A WAYFARER, NOT MUCH MONEY, A TRUE MIDDLE DIFFICULTY.",
      { "WELCOME TO THE FRONTIER. OUT HERE, FORTUNES ARE MADE, EMPIRES",
        "COLLAPSE, AND ENTIRE CIVILIZATIONS HANG IN THE BALANCE. SOME PILOTS",
        "SEEK GLORY. OTHERS SEEK WEALTH, FREEDOM, OR ADVENTURE.",
        "CHAD BEEFSTEEL IS LOOKING FOR A WORKING FUEL INJECTOR.",
        "HIS SHIP IS REGISTERED, TECHNICALLY. HIS DEBTS ARE CONSIDERABLE.",
        "HIS REPUTATION IS MIXED. AND SOMEWHERE IN THE OUTER RIM, SOMEONE",
        "HAS PUT A BOUNTY ON HIS HEAD.",
        "GOOD LUCK, CHAD. YOU'RE GOING TO NEED IT.", 0 } },
    { "REGINALD F-B III", "REGINALD FANSHAWE-BUTTERSWORTH III", DEF_HAULER, 0, 25, -20, 25000, 0,
      "EASY. A BIG UNARMED HAULER AND OLD MONEY. CAN'T DEFEND HIMSELF.",
      { "WELCOME TO THE FRONTIER. OUT HERE, FORTUNES ARE MADE, EMPIRES",
        "COLLAPSE, AND ENTIRE CIVILIZATIONS HANG IN THE BALANCE.",
        "REGINALD FANSHAWE-BUTTERSWORTH III IS LOOKING FOR SOMEONE TO",
        "CARRY HIS BAGS. HE LOST THE FAMILY FLAGSHIP IN A WAGER.",
        "HIS NEW SHIP HAS NO GUNS. HIS ALLOWANCE HAS BEEN CUT OFF, MOSTLY.",
        "THE FSA HAS OPINIONS ABOUT HIS FAMILY.",
        "GOOD LUCK, REGINALD. YOU'RE GOING TO NEED IT.", 0 } },
    { "ZSA-ZSA BLASTWELL", "ZSA-ZSA BLASTWELL", DEF_RAIDER, 3, -30, 10, 3000, 1500,
      "COMBAT. A FAST RAIDER, A SYNDICATE DEBT, AND THE UFS WANTS HER.",
      { "WELCOME TO THE FRONTIER. OUT HERE, FORTUNES ARE MADE, EMPIRES",
        "COLLAPSE, AND ENTIRE CIVILIZATIONS HANG IN THE BALANCE.",
        "ZSA-ZSA BLASTWELL IS LOOKING FOR THE NEXT THING TO OVERTAKE.",
        "HER RACING LICENCE WAS REVOKED AFTER SHE PARKED A FREIGHTER",
        "INSIDE A STATION. THE UFS WOULD LIKE A WORD. SO WOULD THE LENDER",
        "WHO PAID FOR HER SHIP: 1,500 CREDITS, DUE ON DAY 30.",
        "GOOD LUCK, ZSA-ZSA. YOU'RE GOING TO NEED IT.", 0 } },
    { "MARIGOLD Q-PENNYWHISTLE", "PROFESSOR MARIGOLD QUASAR-PENNYWHISTLE", DEF_WAYFARER, 2, -15, 30, 4000, 0,
      "EXPLORER. FSA FRIENDS, A FREE STAR CHART, AND A VERY THIN HULL.",
      { "WELCOME TO THE FRONTIER. OUT HERE, FORTUNES ARE MADE, EMPIRES",
        "COLLAPSE, AND ENTIRE CIVILIZATIONS HANG IN THE BALANCE.",
        "PROFESSOR MARIGOLD QUASAR-PENNYWHISTLE IS LOOKING FOR A COMET.",
        "SHE NAMED IT AFTER HER CAT. THE ACADEMY STRIPPED HER OF HER",
        "TITLE, WHICH SHE STILL USES. HER CHARTS ARE OPTIMISTIC, HER",
        "HULL IS THIN, AND THE CAT IS NOT HELPING.",
        "GOOD LUCK, MARIGOLD. YOU'RE GOING TO NEED IT.", 0 } },
};

enum { T_MENU = 0, T_PILOTS, T_DIFF, T_BRIEF, T_LOAD, T_SOUND };

int title_load_slot;                  /* which save LOAD GAME picked */

/* The difficulty is about the Veyr: every game has them, whoever you fly as. */
static const char *diff_names[DIFF_COUNT] = { "EASY", "NORMAL", "HARD" };
static const char *diff_text[DIFF_COUNT][3] = {
    { "THE VEYR SPREAD SLOWLY, AND THEIR HIVE WORLD",
      "IS EASIER TO DESTROY. TIME TO GET READY.", "" },
    { "THE VEYR INVASION AS IT WAS MEANT TO BE.",
      "COME PREPARED.", "" },
    { "THE VEYR TAKE WORLDS FAST, AND THE HIVE IS A",
      "FORTRESS. KEEP A SPARE CONTROLLER NEARBY.", "UNLESS YOU ARE HIM." },
};

/* The briefing types itself out a word at a time: longer words take a little
 * longer, and the end of a sentence gets a pause. */
static int brief_words, brief_wait, brief_total;

static int words_in(const char *line) {
    int n = 0;
    for (const char *c = line; *c; c++)
        if (*c != ' ' && (c == line || c[-1] == ' ')) n++;
    return n;
}

static int count_words(const Pilot *p) {
    int n = 0;
    for (int i = 0; p->intro[i]; i++) n += words_in(p->intro[i]);
    return n;
}

/* The k-th word overall (0-based): its length, and whether it ends a sentence. */
static int word_info(const Pilot *p, int k, int *ends) {
    for (int i = 0; p->intro[i]; i++) {
        const char *c = p->intro[i];
        while (*c) {
            while (*c == ' ') c++;
            if (!*c) break;
            const char *w = c;
            while (*c && *c != ' ') c++;
            if (k-- == 0) { *ends = c[-1] == '.' || c[-1] == '!' || c[-1] == '?' || c[-1] == ':'; return (int) (c - w); }
        }
    }
    *ends = 0;
    return 0;
}

static void brief_step(const Pilot *p) {
    if (brief_words >= brief_total) return;
    if (brief_wait > 0) { brief_wait--; return; }
    int ends;
    int len = word_info(p, brief_words, &ends);
    brief_words++;
    brief_wait = 3 + len / 2 + (ends ? 22 : 0);
}

/* Draws a line with only the words typed so far; returns how many it used. */
static int draw_typed_line(const char *line, int y, int words_left, int first) {
    char buf[80];
    int n = 0, w = 0;
    const char *c = line;
    while (*c && n < (int) sizeof buf - 1) {
        if (*c != ' ' && (c == line || c[-1] == ' ')) {
            if (w == words_left) break;
            w++;
        }
        buf[n++] = *c++;
    }
    while (n > 0 && buf[n - 1] == ' ') n--;
    buf[n] = 0;
    if (n > 0) gfx_text_big(44, y, 0, 1, first ? 170 : 225, first ? 200 : 228, first ? 230 : 236, buf);
    return w;
}

/* A title sub-screen panel: the battle behind shows through more faintly than in
 * the game's menus, so the text stays easy to read. */
static void dim_panel(int x, int y, int w, int h, int z) {
    gfx_panel(x, y, w, h, z);
    gfx_blend(x + 2, y + 2, w - 4, h - 4, z, GFX_HALF, 0x050814, 0x020308);
}

static void centre(int y, int scale, int r, int g, int b, const char *s) {
    int w = (int) strlen(s) * 8 * scale;
    gfx_text_big((SCREEN_W - w) / 2, y, 0, scale, r, g, b, s);
}

/* ------------------------------------------------------------------------ */
/* The logo (logo_data.c): eight strips, each with its own palette. The glow  */
/* colours carry the mask bit, so they are ADDED to the scene behind.         */
/* ------------------------------------------------------------------------ */

#include <psxgpu.h>
#include <psxgte.h>
extern const u16 logo_left_pix[], logo_right_pix[], logo_cluts[];
static u16 logo_tpage;

static void logo_load(void) {
    RECT a = { 960, 0, 60, 120 }, b = { 960, 128, 60, 120 }, c = { 0, 482, 128, 1 };
    LoadImage(&a, (const uint32_t *) logo_left_pix);  DrawSync(0);
    LoadImage(&b, (const uint32_t *) logo_right_pix); DrawSync(0);
    LoadImage(&c, (const uint32_t *) logo_cluts);     DrawSync(0);
    logo_tpage = getTPage(0, 1, 960, 0);
}

/* bright: 0..128 (128 = full), for the fade-in. */
static void logo_draw(int x, int y, int z, int bright) {
    for (int half = 0; half < 2; half++)
        for (int st = 0; st < 8; st++) {
            POLY_FT4 *q = (POLY_FT4 *) gfx_prim(z, sizeof(POLY_FT4));
            if (!q) return;
            int qx = x + half * 240, qy = y + st * 15, v = half * 128 + st * 15;
            setPolyFT4(q);
            setSemiTrans(q, 1);
            setXY4(q, qx, qy, qx + 240, qy, qx, qy + 15, qx + 240, qy + 15);
            setUV4(q, 0, v, 240, v, 0, v + 15, 240, v + 15);
            setRGB0(q, bright, bright, bright);
            q->tpage = logo_tpage;
            q->clut = getClut(st * 16, 482);
        }
}

/* ------------------------------------------------------------------------ */
/* The battle behind the title: a UFS corvette under attack by Syndicate      */
/* fighters, with its own escorts in pursuit. Real 3D (model.c), not in-game  */
/* flight. Camera space: x right, y up, z into the screen.                    */
/* ------------------------------------------------------------------------ */

#define VIEW_H   460                 /* projection distance */
#define VIEW_CX  (SCREEN_W / 2)
#define VIEW_CY  250
#define FIGHTERS 4
#define BOLTS    20
#define BOOMS    6

typedef struct { ModelPose p; s32 px, pz; int mesh, team, phase, speed, ax, ay, az, fire, dead; } Fighter;
typedef struct { s32 x, y, z, vx, vy, vz; int life, team; } Bolt;
typedef struct { s32 x, y, z; int age, big; } Boom;

static Fighter fighters[FIGHTERS];
static Bolt bolts[BOLTS];
static Boom booms[BOOMS];
static ModelPose capital;
static int beam_timer, beam_target;

/* Angle of the vector (x, z), 0 = +z, growing toward +x; 0..4095. */
static int angle_of(s32 x, s32 z) {
    s32 ax = x < 0 ? -x : x, az = z < 0 ? -z : z;
    if (ax + az == 0) return 0;
    s32 t, a;
    if (ax <= az) { t = ax * 256 / az; a = (512 * t + 178 * t * (256 - t) / 256) >> 8; }
    else          { t = az * 256 / ax; a = 1024 - ((512 * t + 178 * t * (256 - t) / 256) >> 8); }
    if (z < 0) a = 2048 - a;
    if (x < 0) a = 4096 - a;
    return a & 4095;
}

static void boom_at(s32 x, s32 y, s32 z, int big) {
    static int next;
    booms[next] = (Boom) { x, y, z, 1, big };
    next = (next + 1) % BOOMS;
}

static void battle_init(void) {
    static const u8 meshes[FIGHTERS] = { 26, 8, 27, 9 };       /* Scrapper, Talon, Jackal, Vanguard */
    capital = (ModelPose) { -700, -90, 1350, 1210, 0, 0, 4096 * 3 };
    for (int i = 0; i < FIGHTERS; i++) {
        Fighter *f = &fighters[i];
        f->mesh = meshes[i];
        f->team = i & 1;                                       /* 0 raider, 1 escort */
        f->phase = i * 1024 + (int) (rng_next() & 511);
        f->speed = 9 + i * 2;
        f->ax = 620 + i * 90; f->ay = 150 + i * 30; f->az = 420 + i * 60;
        f->fire = 30 + i * 17;
        f->dead = 0;
        f->p.scale = (s16) (4096 * 6 / 10);
    }
    for (int i = 0; i < BOLTS; i++) bolts[i].life = 0;
    for (int i = 0; i < BOOMS; i++) booms[i].age = 0;
    beam_timer = 0;
}

#ifdef FRONTIER_HOST
#include <stdio.h>
#include <stdlib.h>
#endif
static void battle_update(int frame) {
    /* The corvette crosses slowly, then comes round again. */
    capital.x += (frame & 1) ? 2 : 1;
    if (capital.x > 1350) capital.x = -1350;
    capital.bank = (s16) (560 + (isin(frame * 6) >> 6));        /* rolled toward us: we see her decks */

    for (int i = 0; i < FIGHTERS; i++) {
        Fighter *f = &fighters[i];
        if (f->dead) { if (--f->dead == 0) f->phase += 1500; continue; }
        f->phase += f->speed;
        /* A looping path round the corvette: a figure of eight, tilted. */
        s32 x = capital.x + (isin(f->phase) * f->ax >> 12);
        s32 z = capital.z - 420 + (isin(f->phase * 2 + i * 700) * f->az >> 12);
        s32 y = capital.y + 40 + (icos(f->phase + i * 500) * f->ay >> 12);
        /* Fly toward a point well ahead on the path, not along this frame's tiny
         * step (whole-number steps made the nose jitter). Heading, climb and
         * bank all ease toward where they want to be, like a pilot's hands. */
        int ahead = f->phase + f->speed * 12;
        s32 ax = capital.x + (isin(ahead) * f->ax >> 12) - x;
        s32 az = capital.z - 420 + (isin(ahead * 2 + i * 700) * f->az >> 12) - z;
        s32 ay = capital.y + 40 + (icos(ahead + i * 500) * f->ay >> 12) - y;
        s32 vx = x - f->p.x, vz = z - f->p.z;
        int want = angle_of(ax, az);
        int turn = ((want - f->p.heading + 2048) & 4095) - 2048;
        f->p.x = x; f->p.y = y; f->p.z = z;
        f->p.heading = (s16) ((f->p.heading + turn / 5) & 4095);
        int bank = turn * 6;                                   /* lean into the turn */
        if (bank < -800) bank = -800;
        if (bank > 800) bank = 800;
        f->p.bank = (s16) (f->p.bank + (bank - f->p.bank) / 10);
        s32 flat = approx_len(ax, az);
        int climb = flat > 0 ? ay * 900 / flat : 0;
        if (climb < -500) climb = -500;
        if (climb > 500) climb = 500;
        f->p.climb = (s16) (f->p.climb + (climb - f->p.climb) / 10);
        int head = f->p.heading;
        s32 vy = 0;

        if (--f->fire <= 0) {                                  /* a burst along the nose */
            f->fire = 46 + (int) (rng_next() & 31);
            for (int k = 0; k < 2; k++) {
                static int nb;
                Bolt *b = &bolts[nb]; nb = (nb + 1) % BOLTS;
                s32 sn = isin(head), cs = icos(head);
                b->x = x + (k ? 14 : -14); b->y = y; b->z = z;
                b->vx = (sn * 46 >> 12) + vx; b->vy = vy; b->vz = (cs * 46 >> 12) + vz;
                b->life = 40; b->team = f->team;
            }
        }
    }

#ifdef FRONTIER_HOST
    if (getenv("FR_TRACE") && frame > 100 && frame < 400)
        printf("%d %d %d %d\n", frame, fighters[1].p.heading, fighters[1].p.bank, fighters[1].p.climb);
#endif
    for (int i = 0; i < BOLTS; i++) {
        Bolt *b = &bolts[i];
        if (!b->life) continue;
        b->x += b->vx; b->y += b->vy; b->z += b->vz;
        b->life--;
        /* Raiders' shots flash on the corvette's hull when they get there. */
        if (b->team == 0 && b->life && abs32(b->x - capital.x) < 300 && abs32(b->z - capital.z) < 110 && abs32(b->y - capital.y) < 90) {
            boom_at(b->x, b->y, b->z - 60, 0);
            b->life = 0;
        }
    }

    /* The corvette's turret: a beam at a raider now and then; sometimes it kills. */
    if (beam_timer > 0) beam_timer--;
    else if ((frame & 127) == 100) {
        beam_target = (frame >> 7) & 1 ? 2 : 0;
        if (!fighters[beam_target].dead) {
            beam_timer = 9;
            if (((frame >> 7) & 3) == 1) {
                Fighter *f = &fighters[beam_target];
                boom_at(f->p.x, f->p.y, f->p.z, 1);
                f->dead = 150;
            }
        }
    }
    for (int i = 0; i < BOOMS; i++) if (booms[i].age && ++booms[i].age > (booms[i].big ? 44 : 14)) booms[i].age = 0;
}

static int haze(s32 z) { int s = 330 - (int) ((z - 700) / 9); return s > 330 ? 330 : s < 150 ? 150 : s; }

static void battle_draw(int frame) {
    int sx, sy, tx, ty;
    /* Light first (it sits on top of the ships): booms, beam, bolts, engines. */
    for (int i = 0; i < BOOMS; i++) {
        const Boom *b = &booms[i];
        if (!b->age || !model_project(b->x, b->y, b->z, VIEW_CX, VIEW_CY, VIEW_H, &sx, &sy)) continue;
        int span = b->big ? 44 : 14, k = 256 - b->age * 256 / span;           /* 256 -> 0 */
        int r = ((b->big ? 60 : 18) + b->age * (b->big ? 5 : 2)) * VIEW_H / (int) b->z;
        gfx_glow(sx, sy, r, r, LAYER_PROJECTILE, 255 * k >> 8, 190 * k >> 8, 90 * k >> 8);
        if (b->age < span / 3) gfx_glow(sx, sy, r / 2, r / 2, LAYER_PROJECTILE, 255, 255, 255);
        if (b->big) gfx_glow(sx, sy, r * 3, r / 3, LAYER_PROJECTILE, 255 * k >> 9, 120 * k >> 9, 40 * k >> 9);
    }
    if (beam_timer > 0 && !fighters[beam_target].dead || (beam_timer > 0 && fighters[beam_target].dead > 140)) {
        const Fighter *f = &fighters[beam_target];
        if (model_project(capital.x, capital.y + 50, capital.z - 40, VIEW_CX, VIEW_CY, VIEW_H, &sx, &sy) &&
            model_project(f->p.x, f->p.y, f->p.z, VIEW_CX, VIEW_CY, VIEW_H, &tx, &ty)) {
            gfx_line(sx, sy, tx, ty, LAYER_PROJECTILE, 0xFFFFFF, 0xB4E6FF, 1);
            gfx_line(sx, sy + 1, tx, ty + 1, LAYER_PROJECTILE, 0x50B4FF, 0x50B4FF, 1);
            gfx_line(sx + 1, sy, tx + 1, ty, LAYER_PROJECTILE, 0x50B4FF, 0x50B4FF, 1);
            gfx_glow(sx, sy, 16, 16, LAYER_PROJECTILE, 120, 200, 255);
            gfx_glow(tx, ty, 12, 12, LAYER_PROJECTILE, 200, 230, 255);
        }
    }
    for (int i = 0; i < BOLTS; i++) {
        const Bolt *b = &bolts[i];
        if (!b->life) continue;
        if (!model_project(b->x, b->y, b->z, VIEW_CX, VIEW_CY, VIEW_H, &sx, &sy)) continue;
        if (!model_project(b->x - b->vx * 2, b->y - b->vy * 2, b->z - b->vz * 2, VIEW_CX, VIEW_CY, VIEW_H, &tx, &ty)) continue;
        u32 c = b->team ? 0x64C8FF : 0xFF6446;
        gfx_line(sx, sy, tx, ty, LAYER_PROJECTILE, 0xFFFFFF, c, 1);
        gfx_line(sx, sy + 1, tx, ty + 1, LAYER_PROJECTILE, c, 0, 1);
        int g = 5 * VIEW_H / (int) (b->z < 200 ? 200 : b->z) + 3;
        gfx_glow(sx, sy, g, g, LAYER_PROJECTILE, (int) (c >> 16 & 255), (int) (c >> 8 & 255), (int) (c & 255));
    }
    /* Engine glows: behind each fighter's tail, and the corvette's stern. */
    for (int i = 0; i < FIGHTERS; i++) {
        const Fighter *f = &fighters[i];
        if (f->dead) continue;
        s32 sn = isin(f->p.heading), cs = icos(f->p.heading);
        if (!model_project(f->p.x - (sn * 62 >> 12), f->p.y, f->p.z - (cs * 62 >> 12), VIEW_CX, VIEW_CY, VIEW_H, &sx, &sy)) continue;
        int g = 16 * VIEW_H / (int) (f->p.z < 200 ? 200 : f->p.z) + 2 + (frame & 1);
        if (f->team) gfx_glow(sx, sy, g, g, LAYER_PROJECTILE, 90, 170, 255);
        else         gfx_glow(sx, sy, g, g, LAYER_PROJECTILE, 255, 150, 70);
    }
    {
        s32 sn = isin(capital.heading), cs = icos(capital.heading);
        if (model_project(capital.x - (sn * 340 >> 12), capital.y, capital.z - (cs * 340 >> 12), VIEW_CX, VIEW_CY, VIEW_H, &sx, &sy)) {
            int g = 60 * VIEW_H / (int) capital.z + (frame & 1) * 2;
            gfx_glow(sx, sy, g * 2, g, LAYER_PROJECTILE, 90, 170, 255);
        }
    }

    /* The ships, nearest first (a layer draws its latest additions underneath). */
    int idx[FIGHTERS + 1], n = 0;
    s32 depth[FIGHTERS + 1];
    for (int i = 0; i < FIGHTERS; i++) if (!fighters[i].dead) { idx[n] = i; depth[n++] = fighters[i].p.z; }
    idx[n] = -1; depth[n++] = capital.z;
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && depth[j] < depth[j - 1]; j--) {
            s32 d = depth[j]; depth[j] = depth[j - 1]; depth[j - 1] = d;
            int t = idx[j]; idx[j] = idx[j - 1]; idx[j - 1] = t;
        }
    for (int i = 0; i < n; i++) {
        if (idx[i] < 0) model_draw_at(10, &capital, VIEW_CX, VIEW_CY, VIEW_H, LAYER_SHIP, haze(capital.z));   /* UFS Sentinel */
        else {
            const Fighter *f = &fighters[idx[i]];
            if (f->p.z > 140) model_draw_at(f->mesh, &f->p, VIEW_CX, VIEW_CY, VIEW_H, LAYER_SHIP, haze(f->p.z));
        }
    }
}

int title_run(const char *notice, int open_load) {
    int stage = T_MENU, sel = 0, pilot = 0, frame = 0, note_timer = notice ? 300 : 0;
    int diff = DIFF_NORMAL, slot = 0;
    static SaveInfo slots[SAVE_SLOTS];
    if (open_load) { save_scan(slots); stage = T_LOAD; }
    char line[80];
    s32 cx = 0, cy = 0;
    int age = 0;                      /* frames since the title appeared, for the fade-in */
    logo_load();
    battle_init();
    audio_music(SONG_TITLE);          /* M19a */
#ifdef FRONTIER_HOST
    if (getenv("FR_SONG")) audio_music(atoi(getenv("FR_SONG")));   /* preview: one song, or -1 for none */
#endif
    cheats_on = 0;                    /* the code has to be entered again each time (M16b) */
    int code_pos[2] = { 0, 0 };

    for (;;) {
        input_update();
        frame++;
        cx += TO_FP(1) / 2;
        cy -= TO_FP(1) / 4;
        if (note_timer > 0) note_timer--;
        age++;
        battle_update(age);

        /* The cheat code (M16b): UP UP DOWN DOWN LEFT RIGHT LEFT RIGHT X O START, or the short
         * UP DOWN UP DOWN LEFT RIGHT X O START. X, O and START are taken by the code once the
         * arrows are in, so they don't press the menu. */
        int code_ate = 0;
        audio_menu();
#ifdef FRONTIER_HOST
        if (getenv("FR_SFXREEL") && frame % 100 == 50 && frame / 100 < AUD_SFX) audio_sfx(frame / 100);   /* preview: every effect */
#endif
        if (stage == T_MENU && !cheats_on) {
            static const u16 codes[2][11] = {
                { PAD_UP, PAD_UP, PAD_DOWN, PAD_DOWN, PAD_LEFT, PAD_RIGHT, PAD_LEFT, PAD_RIGHT, PAD_CROSS, PAD_CIRCLE, PAD_START },
                { PAD_UP, PAD_DOWN, PAD_UP, PAD_DOWN, PAD_LEFT, PAD_RIGHT, PAD_CROSS, PAD_CIRCLE, PAD_START, 0, 0 } };
            static const u8 code_len[2] = { 11, 9 }, arrows[2] = { 8, 6 };
            static const u16 keys[7] = { PAD_UP, PAD_DOWN, PAD_LEFT, PAD_RIGHT, PAD_CROSS, PAD_CIRCLE, PAD_START };
            for (int k = 0; k < 7; k++) {
                if (!input_pressed(keys[k])) continue;
                for (int c = 0; c < 2; c++) {
                    if (codes[c][code_pos[c]] == keys[k]) {
                        if (code_pos[c] >= arrows[c]) code_ate = 1;
                        if (++code_pos[c] == code_len[c]) {
                            cheats_on = 1;
                            notice = "CHEAT CODE ACCEPTED: DEV MENU ON (START MENU, IN GAME)";
                            note_timer = 300;
                        }
                    } else code_pos[c] = codes[c][0] == keys[k] ? 1 : 0;
                }
            }
            if (cheats_on) code_pos[0] = code_pos[1] = 0;
        }

        if (stage == T_MENU) {
            if (code_ate) { /* the code took this press */ }
            else {
                if (input_pressed(PAD_UP))   sel = (sel + 2) % 3;
                if (input_pressed(PAD_DOWN)) sel = (sel + 1) % 3;
                if (input_pressed(PAD_CROSS | PAD_START)) {
                    if (sel == 0) { stage = T_PILOTS; }
                    else if (sel == 2) { stage = T_SOUND; slot = 0; }
                    else if (save_any()) { save_scan(slots); stage = T_LOAD; slot = 0; }
                    else { notice = "NO FRONTIER SAVES ON THE MEMORY CARD IN SLOT 1"; note_timer = 240; }
                }
            }
        } else if (stage == T_SOUND) {                    /* M19a: the volumes, before you start */
            if (input_pressed(PAD_UP | PAD_DOWN)) slot ^= 1;
            int lr = input_pressed(PAD_RIGHT) ? 1 : input_pressed(PAD_LEFT) ? -1 : 0;
            if (lr) {
                int m = audio_music_volume(), s = audio_sfx_volume();
                if (slot == 0) m += lr; else s += lr;
                audio_set_volume(m, s);
                if (slot == 1) audio_sfx(SFX_CASH);
            }
            if (input_pressed(PAD_CIRCLE | PAD_CROSS)) { stage = T_MENU; slot = 0; }
        } else if (stage == T_PILOTS) {
            if (input_pressed(PAD_LEFT | PAD_UP))    pilot = (pilot + PILOT_COUNT - 1) % PILOT_COUNT;
            if (input_pressed(PAD_RIGHT | PAD_DOWN)) pilot = (pilot + 1) % PILOT_COUNT;
            if (input_pressed(PAD_CIRCLE)) stage = T_MENU;
            if (input_pressed(PAD_CROSS | PAD_START)) stage = T_DIFF;
        } else if (stage == T_DIFF) {
            if (input_pressed(PAD_UP))   diff = (diff + DIFF_COUNT - 1) % DIFF_COUNT;
            if (input_pressed(PAD_DOWN)) diff = (diff + 1) % DIFF_COUNT;
            if (input_pressed(PAD_CIRCLE)) stage = T_PILOTS;
            if (input_pressed(PAD_CROSS | PAD_START)) {
                game_difficulty = diff;
                stage = T_BRIEF; frame = 0;
                brief_words = 0; brief_wait = 20; brief_total = count_words(&pilots[pilot]);
            }
        } else if (stage == T_LOAD) {
            if (input_pressed(PAD_UP))   slot = (slot + SAVE_SLOTS - 1) % SAVE_SLOTS;
            if (input_pressed(PAD_DOWN)) slot = (slot + 1) % SAVE_SLOTS;
            if (input_pressed(PAD_CIRCLE)) stage = T_MENU;
            if (input_pressed(PAD_CROSS | PAD_START)) {
                if (slots[slot].used) { title_load_slot = slot; return TITLE_LOAD; }
                notice = "THAT SLOT IS EMPTY"; note_timer = 120;
            }
        } else {
            if (input_pressed(PAD_CIRCLE)) stage = T_DIFF;
            else if (input_pressed(PAD_CROSS | PAD_START)) {
                if (brief_words < brief_total) brief_words = brief_total;   /* first press: show it all */
                else return pilot;
            }
            brief_step(&pilots[pilot]);
        }

        /* --- drawing --- */
        if (stage == T_MENU) {
            int fade = age < 64 ? age * 2 : 128;
            logo_draw((SCREEN_W - 480) / 2, 34, 0, fade);
            if (age > 50) centre(152, 1, 150, 200, 235, "A  SPACE  TRADING  AND  COMBAT  RPG");
            if (age > 70) {
                gfx_button(SCREEN_W / 2 - 110, 330, 220, 26, 0, sel == 0 ? BTN_SELECTED : BTN_NORMAL, 2, "NEW GAME");
                gfx_button(SCREEN_W / 2 - 110, 360, 220, 26, 0, sel == 1 ? BTN_SELECTED : BTN_NORMAL, 2, "LOAD GAME");
                gfx_button(SCREEN_W / 2 - 110, 390, 220, 26, 0, sel == 2 ? BTN_SELECTED : BTN_NORMAL, 2, "SOUND");
                centre(438, 1, 120, 134, 160, "HOMEBREW FOR PLAYSTATION. MADE WITH THE HELP OF AI.");
                centre(452, 1, 150, 160, 180, "X: SELECT");
            }
        } else if (stage == T_SOUND) {
            logo_draw((SCREEN_W - 480) / 2, 34, 0, 128);
            char b[40];
            snprintf(b, sizeof b, "MUSIC    < %2d >", audio_music_volume());
            gfx_button(SCREEN_W / 2 - 150, 330, 300, 26, 0, slot == 0 ? BTN_SELECTED : BTN_NORMAL, 2, b);
            snprintf(b, sizeof b, "EFFECTS  < %2d >", audio_sfx_volume());
            gfx_button(SCREEN_W / 2 - 150, 360, 300, 26, 0, slot == 1 ? BTN_SELECTED : BTN_NORMAL, 2, b);
            centre(404, 1, 150, 160, 180, "UP DOWN: CHOOSE   LEFT RIGHT: LOUDER, QUIETER   O: BACK");
            centre(420, 1, 120, 134, 160, "0 IS OFF. A LOADED GAME BRINGS ITS OWN SETTINGS.");
        } else if (stage == T_PILOTS) {
            const Pilot *p = &pilots[pilot];
            centre(40, 3, 90, 220, 255, "CHOOSE YOUR PILOT");
            static const char *firsts[PILOT_COUNT] = { "CHAD", "REGINALD", "ZSA-ZSA", "MARIGOLD" };
            for (int i = 0; i < PILOT_COUNT; i++)
                gfx_button(36 + i * 144, 90, 138, 22, 0, i == pilot ? BTN_SELECTED : BTN_NORMAL, 2, firsts[i]);
            gfx_text_big(44, 140, 0, 2, 255, 220, 120, p->full_name[0] && strlen(p->full_name) <= 34 ? p->full_name : p->name);
            gfx_text_big(44, 172, 0, 1, 170, 220, 170, p->tag);
            snprintf(line, sizeof line, "SHIP: %s", ship_looks[p->ship].name);
            gfx_text_big(44, 200, 0, 1, 220, 225, 235, line);
            snprintf(line, sizeof line, "CREDITS: %d%s", (int) p->credits, p->debt ? "  (DEBT 1500, DUE DAY 30)" : "");
            gfx_text_big(44, 216, 0, 1, 220, 225, 235, line);
            snprintf(line, sizeof line, "STARTS IN: %s (%s)", galaxy_sys(p->start_sys)->name,
                     galaxy_sys(p->start_sys)->owner == 0 ? "UFS" : galaxy_sys(p->start_sys)->owner == 2 ? "FSA" :
                     galaxy_sys(p->start_sys)->owner == 3 ? "SYNDICATE" : "INDEPENDENT");
            gfx_text_big(44, 232, 0, 1, 220, 225, 235, line);
            snprintf(line, sizeof line, "STANDING: UFS %+d   FSA %+d", p->ufs, p->fsa);
            gfx_text_big(44, 248, 0, 1, 220, 225, 235, line);
            centre(420, 1, 150, 160, 180, "LEFT RIGHT: CHOOSE   X: THIS PILOT   O: BACK");
            dim_panel(24, 24, SCREEN_W - 48, 252, 0);
        } else if (stage == T_DIFF) {
            centre(40, 3, 90, 220, 255, "THE VEYR");
            centre(76, 1, 170, 190, 215, "THEY COME FOR EVERY PILOT. HOW HARD SHOULD THEY BE?");
            for (int i = 0; i < DIFF_COUNT; i++)
                gfx_button(SCREEN_W / 2 - 110, 104 + i * 32, 220, 26, 0, i == diff ? BTN_SELECTED : BTN_NORMAL, 2, diff_names[i]);
            for (int k = 0; k < 3; k++)
                if (diff_text[diff][k][0])
                    centre(214 + k * 16, 1, k == 2 ? 255 : 220, k == 2 ? 120 : 225, k == 2 ? 100 : 235, diff_text[diff][k]);
            centre(420, 1, 150, 160, 180, "UP DOWN: CHOOSE   X: THIS ONE   O: BACK");
            dim_panel(24, 24, SCREEN_W - 48, 252, 0);
        } else if (stage == T_LOAD) {
            centre(40, 3, 90, 220, 255, "LOAD GAME");
            for (int i = 0; i < SAVE_SLOTS; i++) {
                const SaveInfo *si = &slots[i];
                char pt[16];
                int y = 92 + i * 44;
                if (si->used) {
                    save_play_time(pt, sizeof pt, si->play_frames);
                    snprintf(line, sizeof line, "%-8s %s", save_slot_name(i), si->name);
                    gfx_button(48, y, SCREEN_W - 96, 22, 0, i == slot ? BTN_SELECTED : BTN_NORMAL, 2, line);
                    snprintf(line, sizeof line, "DAY %d   PLAYED %s   %s   %s   %d CR", si->day, pt,
                             si->sys < GALAXY_SYSTEMS ? galaxy_sys(si->sys)->name : "?",
                             diff_names[si->difficulty % DIFF_COUNT], (int) si->credits);
                    gfx_text_big(64, y + 26, 0, 1, 150, 200, 160, line);
                } else {
                    snprintf(line, sizeof line, "%-8s EMPTY", save_slot_name(i));
                    gfx_button(48, y, SCREEN_W - 96, 22, 0, i == slot ? BTN_SELECTED : BTN_DISABLED, 2, line);
                }
            }
            centre(420, 1, 150, 160, 180, "UP DOWN: CHOOSE   X: LOAD   O: BACK");
            dim_panel(24, 24, SCREEN_W - 48, 276, 0);
        } else {
            const Pilot *p = &pilots[pilot];
            int left = brief_words, last_line = 0, last_len = 0;
            for (int i = 0; p->intro[i] && left > 0; i++) {
                left -= draw_typed_line(p->intro[i], 120 + i * 22, left, i == 0);
                last_line = i;
            }
            if (brief_words < brief_total) {
                /* a blinking cursor just after the last word typed */
                int l = brief_words;
                for (int i = 0; i < last_line; i++) l -= words_in(p->intro[i]);
                const char *c = p->intro[last_line];
                for (int w = 0; *c; c++) {
                    if (*c != ' ' && (c == p->intro[last_line] || c[-1] == ' ')) { if (w == l) break; w++; }
                    last_len++;
                }
                while (last_len > 0 && p->intro[last_line][last_len - 1] == ' ') last_len--;
                if ((frame >> 3) & 1) gfx_rect(44 + last_len * 8 + (l ? 4 : 0), 120 + last_line * 22, 6, 9, 0, 225, 228, 236);
                centre(420, 1, 150, 160, 180, "X: SKIP   O: BACK");
            } else {
                centre(420, 1, 150, 160, 180, "X: LAUNCH   O: BACK");
            }
            dim_panel(24, 96, SCREEN_W - 48, 230, 0);
        }
        if (note_timer > 0 && notice) centre(stage == T_MENU ? 422 : 370, 1, 255, 200, 90, notice);   /* under the buttons */

        battle_draw(age);
        world_draw_planet(TO_FP(-190), TO_FP(-250));     /* Aurora's limb, low on the left */
        world_draw_stars(cx, cy);
        gfx_flip();
    }
}
