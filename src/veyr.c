#include <stdio.h>
#include <string.h>
#include <psxpad.h>

#include "audio.h"
#include "conquest.h"
#include "enemy.h"
#include "faction.h"
#include "galaxy.h"
#include "gfx.h"
#include "input.h"
#include "outfit.h"
#include "rng.h"
#include "save.h"
#include "clock.h"
#include "story.h"
#include "ship.h"
#include "veyr.h"
#include "world.h"

static u8  phase, ending, revealed, contain;
static u16 spread_hours, kills;
static u16 end_day, gift_day;          /* the day the war ended; the hive's last gift (M18b) */
static u8  former[GALAXY_SYSTEMS];    /* who held each system before the Veyr took it */
static char msg[80];

/* The banner: two lines, shown for a few seconds over everything. */
static char bc_head[40], bc_line[2][64];
static int  bc_timer;
static void broadcast(const char *head, const char *l1, const char *l2);

void veyr_banner(const char *head, const char *l1, const char *l2) { broadcast(head, l1, l2); }
static void broadcast(const char *head, const char *l1, const char *l2) {
    snprintf(bc_head, sizeof bc_head, "%s", head);
    snprintf(bc_line[0], sizeof bc_line[0], "%s", l1 ? l1 : "");
    snprintf(bc_line[1], sizeof bc_line[1], "%s", l2 ? l2 : "");
    bc_timer = 480;
    audio_sfx(SFX_BANNER);
}

void veyr_init(void) {
    phase = VP_DORMANT; ending = VEYR_END_NONE; revealed = 0; end_day = gift_day = 0;
    spread_hours = 0; kills = 0; bc_timer = 0; contain = 0;
    for (int i = 0; i < GALAXY_SYSTEMS; i++) former[i] = galaxy_sys(i)->owner;
}

int veyr_phase(void)    { return phase; }
int veyr_ending(void)   { return ending; }
int veyr_revealed(void) { return revealed; }
int veyr_kills(void)    { return kills; }

/* ---- Difficulty: how fast they spread, how often humans win a world back ---- */

static int spread_interval(void) { static const u8 h[3] = { 36, 18, 10 }; return h[game_difficulty % 3]; }
static int spread_count(void)    { int d = game_difficulty % 3; return d == 0 ? 1 : d == 1 ? 1 + rng_range(2) : 2 + rng_range(2); }
static int counter_pct(void)     { static const u8 p[3] = { 30, 18, 8 }; return p[game_difficulty % 3]; }

static int is_human_port(int s) {
    const StarSystem *g = galaxy_sys(s);
    return !g->hidden && g->station_name && g->owner != OWN_VEYR;
}

static int human_ports_left(void) {
    int n = 0;
    for (int i = 0; i < GALAXY_SYSTEMS; i++) n += is_human_port(i);
    return n;
}

static int touches_veyr(int s) {
    const StarSystem *g = galaxy_sys(s);
    for (int j = 0; j < g->njumps; j++) if (galaxy_sys(g->link[j])->owner == OWN_VEYR) return 1;
    return 0;
}

static const char *who_held(int own) {
    return own == OWN_FEDERATION ? "UFS" : own == OWN_REBEL ? "FSA" : own == OWN_SYNDICATE ? "SYNDICATE" :
           own == OWN_PLAYER ? "YOUR" : "INDEPENDENT";
}

/* Your ships in a system the Veyr come for (M17a): a squadron holding it fights
 * (strong enough and the Veyr are thrown back); ships parked in its hangar flee
 * to a neighbouring port, if there is one. Returns 1 if the system held. */
static int defend(int s) {
    int power = 0, any = 0;
    for (int i = 0; i < fleet_count; i++)
        if (fleet_sys[i] == s && fleet_sq[i] != 0 && sq_order[fleet_sq[i]] != SQ_MOVE) { power += fleet_strength(i); any = 1; }
    int swarm = 2500 + 1500 * (game_difficulty % 3) + rng_range(1500);
    if (any && power >= swarm) {
        snprintf(msg, sizeof msg, "YOUR SQUADRON HELD %s AGAINST THE VEYR", galaxy_sys(s)->name);
        broadcast("THE LINE HOLDS", msg, "THE SWARM BREAKS ON YOUR GUNS AND FALLS BACK.");
        for (int i = 0; i < fleet_count; i++)
            if (fleet_sys[i] == s && fleet_sq[i] != 0) fleet_scored(i);
        return 1;
    }
    int refuge = -1;
    const StarSystem *g = galaxy_sys(s);
    for (int j = 0; j < g->njumps; j++) if (galaxy_port_open(g->link[j])) { refuge = g->link[j]; break; }
    for (int i = fleet_count - 1; i >= 0; i--) {
        if (fleet_sys[i] != s || (fleet_sq[i] == 0 && fleet_stance[i] != STANCE_HANGAR) || s == galaxy_current()) continue;
        if (fleet_sq[i] != 0 && sq_order[fleet_sq[i]] == SQ_MOVE) continue;
        if (fleet_sq[i] == 0 && refuge >= 0) fleet_sys[i] = (u8) refuge;      /* parked: it runs */
        else fleet_remove(i);                                                  /* a squadron that lost */
    }
    return 0;
}

/* The Veyr take a system: the owner is remembered, so it can be given back. */
static const char *take(int s) {
    int own = galaxy_sys(s)->owner;
    if (defend(s)) return msg;
    former[s] = (u8) own;
    galaxy_set_owner(s, OWN_VEYR);
    if (s == galaxy_current()) {                       /* you are there: the swarm is on you */
        conquest_abort_quiet();
        enemies_set_system(6, 0, 0, FAC_NONE);
        enemies_set_veyr(100);
    }
    if (galaxy_sys(s)->station_name == 0) return 0;    /* empty space: nobody to mourn */
    if (own == OWN_PLAYER) snprintf(msg, sizeof msg, "YOUR WORLD %s HAS FALLEN TO THE VEYR", galaxy_sys(s)->name);
    else snprintf(msg, sizeof msg, "%s HAS FALLEN TO THE VEYR (%s)", galaxy_sys(s)->name, who_held(own));
    broadcast("EMERGENCY BROADCAST", msg, own == OWN_PLAYER ? "THE TRIBUTE FROM IT HAS STOPPED." : "ALL SHIPS: AVOID THE SYSTEM.");
    return msg;
}

const char *veyr_begin(void) {
    if (phase != VP_DORMANT) return "THE VEYR ARE ALREADY HERE";
    phase = VP_INVADING;
    spread_hours = (u16) spread_interval();
    take(VEYR_DOOR_A);
    take(VEYR_DOOR_B);
    snprintf(msg, sizeof msg, "THE VEYR HAVE CROSSED THE RIM AT %s AND %s",
             galaxy_sys(VEYR_DOOR_A)->name, galaxy_sys(VEYR_DOOR_B)->name);
    broadcast("EMERGENCY BROADCAST: ALL CHANNELS", msg, "THIS IS NOT A DRILL. GOD HELP US ALL.");
    return msg;
}

const char *veyr_frame(void) {
    if (bc_timer > 0) bc_timer--;
    if (bc_timer > 0 && bc_timer < 400 &&                 /* read it? any button clears it */
        input_pressed(PAD_CROSS | PAD_CIRCLE | PAD_SQUARE | PAD_TRIANGLE | PAD_START)) bc_timer = 0;
    if (phase == VP_DORMANT && play_frames >= VEYR_DEADLINE) return veyr_begin();
    return 0;
}

/* One step of the spread: a few of the systems on the Veyr border fall. */
static const char *spread(void) {
    const char *m = 0;
    int want = spread_count();
    for (int k = 0; k < want; k++) {
        int pick[GALAXY_SYSTEMS], n = 0;
        for (int i = 0; i < GALAXY_SYSTEMS; i++) {
            const StarSystem *g = galaxy_sys(i);
            if (g->owner == OWN_VEYR || g->hidden || !touches_veyr(i)) continue;
            pick[n++] = i;
            if (g->station_name) pick[n++] = i;        /* they go for worlds before empty space */
        }
        if (!n) break;
        const char *t = take(pick[rng_range(n)]);
        if (t) m = t;
    }

    /* Sometimes the humans win one back: a world next to its old owner's space. */
    if (rng_range(100) < counter_pct()) {
        int pick[GALAXY_SYSTEMS], n = 0;
        for (int i = 0; i < GALAXY_SYSTEMS; i++) {
            const StarSystem *g = galaxy_sys(i);
            int f = former[i];
            if (g->owner != OWN_VEYR || g->hidden || f == OWN_PLAYER || f == OWN_NONE || f == OWN_VEYR) continue;
            for (int j = 0; j < g->njumps; j++)
                if (galaxy_sys(g->link[j])->owner == f) { pick[n++] = i; break; }
        }
        if (n) {
            int s = pick[rng_range(n)];
            galaxy_set_owner(s, former[s]);
            static char back[64];
            snprintf(back, sizeof back, "THE %s HAVE RETAKEN %s FROM THE VEYR", who_held(former[s]), galaxy_sys(s)->name);
            if (!m) m = back;
        }
    }
    if (human_ports_left() == 0) phase = VP_LOST;
    return m;
}

const char *veyr_hour(void) {
    if (phase != VP_INVADING) return 0;
    if (spread_hours > 0 && --spread_hours > 0) return 0;
    spread_hours = (u16) spread_interval();
    return spread();
}

const char *veyr_spread_now(void) {
    if (phase != VP_INVADING) return "THE VEYR ARE NOT INVADING";
    const char *m = spread();
    return m ? m : "THE VEYR SPREAD THROUGH EMPTY SPACE";
}

const char *veyr_reveal(const char *how) {
    if (revealed) return 0;
    revealed = 1;
    galaxy_open_nebula(1);
    galaxy_mark_known(VEYR_NEBULA);
    snprintf(msg, sizeof msg, "%s: THE HIVE LIES PAST %s", how, galaxy_sys(VEYR_DOOR_A)->name);
    broadcast("THE HIVE IS FOUND", msg, "THE NEBULA IS OPEN. VEYR PRIME IS AT ITS HEART.");
    return msg;
}

void veyr_on_kill(void) {
    if (kills < 9999) kills++;
    if (phase == VP_INVADING && !revealed && kills >= VEYR_REVEAL_KILLS)
        veyr_reveal("VEYR NAV CORES DECODED");
}

static int choice_open, choice_sel, choice_confirm;

const char *veyr_on_capture(int sys, int prev_owner) {
    if (prev_owner != OWN_VEYR) return 0;
    if (sys == VEYR_PRIME) { choice_open = 1; choice_sel = 0; choice_confirm = 0; return 0; }
    if (!revealed && phase == VP_INVADING) {
        static char how[48];
        snprintf(how, sizeof how, "A NAV CORE IN THE RUINS OF %s", galaxy_sys(sys)->name);
        return veyr_reveal(how);
    }
    return 0;
}

int veyr_swarm_chance(int sys) {
    const StarSystem *g = galaxy_sys(sys);
    if (g->owner == OWN_VEYR) return 100;
    if (phase == VP_INVADING && touches_veyr(sys)) return 40;      /* the border: raids from next door */
    return 0;
}

/* ------------------------------------------------------------------------ */
/* The hive choice                                                           */
/* ------------------------------------------------------------------------ */

int veyr_choice_active(void) { return choice_open; }

/* Burning the hive serves whoever you swore to: the FSA if it was them, else the UFS (M18b). */
static int burn_for_fsa(void) { return story_sworn() == TRACK_FSA; }
static const char *decide(int how);
const char *veyr_dev_end(int how) {
    if (phase == VP_DORMANT) veyr_begin();
    if (!revealed) veyr_reveal("DEV MENU");
    bc_timer = 0;
    return decide(how);
}

int veyr_hero_side(void) {
    return ending == VEYR_END_XENOCIDE ? OWN_FEDERATION : ending == VEYR_END_LIBERATION ? OWN_REBEL :
           ending == VEYR_END_SEIZED ? OWN_SYNDICATE : -1;
}
int veyr_hero_port(int sys) {
    int side = veyr_hero_side();
    if (side < 0 || !galaxy_port_open(sys)) return 0;
    const StarSystem *g = galaxy_sys(sys);
    if (g->owner == OWN_PLAYER) return 1;                               /* your own worlds too */
    if (side == OWN_SYNDICATE) return galaxy_founder(sys) == OWN_SYNDICATE && g->owner != OWN_FEDERATION && g->owner != OWN_REBEL;
    return g->owner == side;
}
const char *veyr_title(void) {
    return ending == VEYR_END_XENOCIDE ? "ADMIRAL OF THE UFS" : ending == VEYR_END_LIBERATION ? "MARSHAL OF THE FSA" :
           ending == VEYR_END_SEIZED ? "THE RIM CROWN" : 0;
}
const char *veyr_medal(void) {
    return ending == VEYR_END_XENOCIDE ? "THE AURORA STAR" : ending == VEYR_END_LIBERATION ? "THE LIBERTY STAR" :
           ending == VEYR_END_SEIZED ? "THE HIVE MIND" : 0;
}
/* The nebula's worlds are yours whatever you chose. Burned, they stand empty and
 * settlers come over ten days; seized, the swarm works them from day one. */
int veyr_settled_pct(int sys) {
    if (!galaxy_sys(sys)->hidden) return 100;
    if (ending != VEYR_END_XENOCIDE && ending != VEYR_END_LIBERATION) return 100;
    int d = clock_day() - end_day;
    return d >= 10 ? 100 : d <= 0 ? 0 : d * 10;
}
/* The hive grows its master a ship every third day, if the fleet has room. */
const char *veyr_day(void) {
    if (!veyr_hive_mine() || clock_day() - gift_day < 3) return 0;
    gift_day = (u16) clock_day();
    if (fleet_count >= FLEET_MAX) return "THE HIVE GREW A SHIP, BUT YOUR FLEET IS FULL";
    int r = rng_range(100);
    int def = r < 45 ? DEF_SKITTER : r < 80 ? DEF_MANTIS : r < 97 ? DEF_REAVER : DEF_BROODGUARD;
    fleet_add(def);
    fleet_sys[fleet_count - 1] = VEYR_PRIME;                           /* grown at the hive: it waits there */
    snprintf(msg, sizeof msg, "THE HIVE GREW YOU A %s. IT WAITS AT VEYR PRIME", ship_looks[def].name);
    return msg;
}

/* Every world the Veyr hold falls back to whoever held it; the nebula is yours. */
static void collapse(void) {
    for (int i = 0; i < GALAXY_SYSTEMS; i++) {
        if (galaxy_sys(i)->owner != OWN_VEYR) continue;
        int f = former[i];
        galaxy_set_owner(i, galaxy_sys(i)->hidden || f == OWN_VEYR ? OWN_PLAYER : f);
    }
    galaxy_set_owner(VEYR_PRIME, OWN_PLAYER);
    phase = VP_BEATEN;
}

const char *veyr_choice_update(void) {
    if (!choice_open) return 0;
    bc_timer = 0;                                       /* nothing talks over this */
    audio_menu();
    if (input_pressed(PAD_LEFT | PAD_UP | PAD_RIGHT | PAD_DOWN)) { choice_sel ^= 1; choice_confirm = 0; }
    if (!input_pressed(PAD_CROSS)) return 0;
    if (!choice_confirm) { choice_confirm = 1; return 0; }
    choice_open = 0;
    return decide(choice_sel == 0 ? (burn_for_fsa() ? 1 : 0) : 2);
}

/* The war ends: 0 burned for the UFS, 1 burned for the FSA, 2 seized (M18b). */
static const char *decide(int how) {
    collapse();
    end_day = gift_day = (u16) clock_day();
    if (how == 1) {           /* M18b: burned for the Free States */
        ending = VEYR_END_LIBERATION;
        faction_set(FAC_REBEL, 100);
        faction_set(FAC_FEDERATION, -80);
        syn_set(SYN_MARKED - 15);
        broadcast("THE VEYR ARE GONE", "THE HIVE BURNS. MARSHAL, THE LIBERTY STAR IS YOURS.",
                  "NEW ORDERS: FREE EVERY WORLD THE UFS HOLDS.");
        return "THE HIVE BURNS. YOU ARE A MARSHAL OF THE FSA";
    }
    if (how == 0) {
        ending = VEYR_END_XENOCIDE;
        faction_set(FAC_FEDERATION, 100);
        faction_set(FAC_REBEL, -80);
        syn_set(SYN_MARKED - 15);
        /* M18b: no free Sovereign any more (UFS yards sell it at 90): the Aurora Star instead. */
        broadcast("THE VEYR ARE GONE", "THE HIVE BURNS. ADMIRAL, THE AURORA STAR IS YOURS.",
                  "NEW ORDERS: BRING EVERY FSA WORLD TO HEEL.");
        return "XENOCIDE. YOU ARE A UFS ADMIRAL NOW";
    }
    ending = VEYR_END_SEIZED;
    faction_set(FAC_FEDERATION, -100);
    faction_change(FAC_REBEL, 25);
    syn_set(100);
    broadcast("THE VEYR ARE YOURS", "THE HIVE ANSWERS TO YOU. THE RIM CROWN IS YOURS.",
              "THE SWARM AND THE SYNDICATES: TAKE THE GALAXY.");
    return "THE HIVE IS YOURS. THE UFS WANTS YOU DEAD";
}

static void ctext(int y, int scale, int r, int g, int b, const char *t) {
    int w = (int) strlen(t) * 8 * scale;
    gfx_text_big((SCREEN_W - w) / 2, y, 0, scale, r, g, b, t);
}

void veyr_choice_draw(void) {
    if (!choice_open) return;
    ctext(70, 3, 150, 230, 60, "THE HIVE MIND");
    ctext(130, 1, 210, 215, 225, "VEYR PRIME IS YOURS. THE HIVE MIND LIES OPEN UNDER YOUR GUNS.");
    ctext(146, 1, 210, 215, 225, "EVERY VEYR SHIP IN THE GALAXY ANSWERS TO IT. WHAT NOW?");
    static const char *ufs[4] = { "XENOCIDE: BURN THE HIVE", "THE UFS ORDER. THE VEYR DIE, ALL OF THEM.",
          "YOU BECOME AN ADMIRAL OF THE UFS. THE FSA IS NEXT.", "THE FSA WILL HATE YOU. THE SYNDICATES WILL SCORN YOU." };
    static const char *fsa[4] = { "BURN THE HIVE: FOR THE FREE STATES", "NOBODY ORDERED IT. THE FSA WON'T MOURN THEM.",
          "YOU BECOME A MARSHAL OF THE FSA. THE UFS IS NEXT.", "THE UFS WILL HATE YOU. THE SYNDICATES WILL SCORN YOU." };
    static const char *keep[4] = { "SEIZE THE HIVE", "KEEP THE HIVE, ITS TECH AND ITS SWARMS.",
          "THE RIM CROWN IS YOURS. THE GALAXY IS NEXT.", "THE UFS BRANDS YOU A TRAITOR. THE RIM BOWS." };
    const char *const *opt[2] = { burn_for_fsa() ? fsa : ufs, keep };
    for (int k = 0; k < 2; k++) {
        int x = 60, y = 180 + k * 104;
        gfx_text_big(x + 16, y + 10, 0, 2, k == choice_sel ? 255 : 170, k == choice_sel ? 230 : 180, k == choice_sel ? 90 : 200, opt[k][0]);
        for (int l = 1; l < 4; l++) gfx_text_big(x + 16, y + 26 + l * 16, 0, 1, 180, 190, 205, opt[k][l]);
        gfx_button(x, y, SCREEN_W - 120, 92, 0, k == choice_sel ? BTN_SELECTED : BTN_NORMAL, 1, "");
    }
    ctext(400, 1, 150, 160, 180, choice_confirm ? "PRESS X AGAIN TO DECIDE. THERE IS NO GOING BACK."
                                                : "UP/DOWN: CHOOSE   X: DECIDE");
    gfx_blend(0, 0, SCREEN_W, SCREEN_H, 0, GFX_HALF, 0x000000, 0x000000);
}

/* ------------------------------------------------------------------------ */
/* The emergency broadcast banner                                            */
/* ------------------------------------------------------------------------ */

void veyr_broadcast_draw(void) {
    if (bc_timer <= 0) return;
    int y = 150;
    int on = (bc_timer >> 4) & 1;
    ctext(y + 10, 2, on ? 255 : 200, on ? 70 : 50, 50, bc_head);
    ctext(y + 40, 1, 230, 230, 235, bc_line[0]);
    ctext(y + 56, 1, 200, 205, 215, bc_line[1]);
    gfx_rect(40, y, SCREEN_W - 80, 2, 0, 255, 70, 50);
    gfx_rect(40, y + 76, SCREEN_W - 80, 2, 0, 255, 70, 50);
    gfx_rect(40, y + 2, SCREEN_W - 80, 74, 0, 16, 6, 6);           /* solid: it must be read */
}

/* ------------------------------------------------------------------------ */
/* The end of the line                                                       */
/* ------------------------------------------------------------------------ */

void veyr_game_over(void) {
    static const char *roll[] = {
        "THE LAST HUMAN PORT HAS FALLEN.", "", "THE VEYR HOLD EVERY WORLD FROM THE CORE TO THE RIM.",
        "SOMEWHERE A BEACON STILL BROADCASTS, TO NO ONE.", "", "", "FRONTIER", "",
        "A GAME BY XENOCIDE", "", "WITH CLAUDE (ANTHROPIC)", "CODE, SHIPS AND WORDS", "",
        "AND FABLE", "THE GRAPHICS OVERHAUL", "", "BUILT WITH PSNOOBSDK", "",
        "THANK YOU FOR PLAYING.", "", "", "THE FRONTIER IS LOST.", "BUT THERE IS ALWAYS ANOTHER GAME." };
    int n = (int) (sizeof roll / sizeof roll[0]);
    int total = SCREEN_H + n * 24 + 120;
    audio_music(SONG_VEYR);
    for (int t = 0; t < total; t++) {
        input_update();
        if (t > 120 && input_pressed(PAD_START | PAD_CROSS)) break;
        for (int i = 0; i < n; i++) {
            int y = SCREEN_H - t + i * 24;
            if (y < -16 || y > SCREEN_H) continue;
            int big = i == 6;
            ctext(y, big ? 3 : 1, big ? 150 : 210, big ? 230 : 215, big ? 60 : 225, roll[i]);
        }
        world_draw_stars(TO_FP(t), 0);
        gfx_flip();
    }
}

int  veyr_contain(void) { return contain; }
void veyr_contain_set(int lv) { contain = (u8) (lv < 0 ? 0 : lv > CONTAIN_MAX ? CONTAIN_MAX : lv); }
int  veyr_contain_price(int lv) { static const int p[CONTAIN_MAX + 1] = { 0, 20000, 50000, 120000 }; return p[lv & 3]; }
int  veyr_is_ship(int def) { return def >= DEF_SKITTER && def <= DEF_LEVIATHAN; }
int  veyr_contain_needs(int def) { return def <= DEF_MANTIS ? 1 : def == DEF_REAVER ? 2 : 3; }
int  veyr_claim_risk(int def) {
    if (contain < veyr_contain_needs(def)) return 100;          /* the hull rejects you, every time */
    return 60 - 15 * contain;                                   /* 45, 30, 15% */
}
int  veyr_hive_mine(void) { return ending == VEYR_END_SEIZED && galaxy_sys(VEYR_PRIME)->owner == OWN_PLAYER; }

u8 *veyr_ser(u8 *p, int load) {
    SER(p, load, phase); SER(p, load, ending); SER(p, load, revealed);
    if (!load || ser_version >= 10) SER(p, load, contain); else contain = 0;
    SER(p, load, spread_hours); SER(p, load, kills); SER(p, load, former);
    if (!load || ser_version >= 12) { SER(p, load, end_day); SER(p, load, gift_day); }
    else { end_day = gift_day = 0; }
    if (load) { galaxy_open_nebula(revealed); bc_timer = 0; choice_open = 0; }
    return p;
}
