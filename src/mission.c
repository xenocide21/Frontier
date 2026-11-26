/*
 * FRONTIER - missions, the mercenary guild and discoveries (Milestone 13j). See mission.h.
 */
#include <stdio.h>

#include "audio.h"
#include "clock.h"
#include "enemy.h"
#include "faction.h"
#include "galaxy.h"
#include "gfx.h"
#include "market.h"
#include "mission.h"
#include "story.h"
#include "save.h"
#include "outfit.h"
#include "rng.h"
#include "world.h"

static Mission offers[MISSION_OFFERS], active[MISSION_ACTIVE];
static int n_offers, n_active;
static u32 board_key, taken[16];
static int n_taken;
static int last_hour, last_kills;
static const char *bounty_name_now;
static char msg[64];

/* One hidden anomaly per system (placeholder positions, well clear of everything). */
/* One per system, generated with the galaxy (src/galaxy_data.c, M16a). */
extern const s16 galaxy_anomaly_xy[GALAXY_SYSTEMS][2];
extern const char *const galaxy_anomaly_name[GALAXY_SYSTEMS];
#define anomaly_xy   galaxy_anomaly_xy
#define anomaly_name galaxy_anomaly_name
#define ANOMALY_RANGE 220
#define DATA_PRICE    1800
static u8 discovered[GALAXY_SYSTEMS], data_sold[GALAXY_SYSTEMS];

static const char *wanted_names[16] = {
    "RED VASKO", "ONE-EYED MARGO", "DUSTY KRELL", "THE BARON OF SCRAP",
    "SILKY JOHANSEN", "MAD DOG ORTEGA", "GRANNY HEX", "BIG TEDDY VOSS",
    /* 8..: the story's (M18a) */
    "THE FOX", "LT. GRAYSON", "SLIPPERY DEKE", "INSPECTOR MOLL", "", "", "", "" };
static const char *type_names[MI_TYPES] = {
    "", "CARGO", "COURIER", "EMERGENCY SUPPLY", "PASSENGERS", "BOUNTY", "INTERCEPT", "SMUGGLING", "SURVEY",
    "CAPTURE", "CLAIM" };
static const char *giver_names[4] = { "UFS", "FSA", "INDEPENDENT", "SYNDICATE" };

static int now_hours(void) { return (clock_day() - 1) * 24 + clock_hour(); }

void missions_init(void) {
    n_offers = n_active = n_taken = 0;
    board_key = 0;
    last_hour = -1;
    last_kills = enemies_player_pirate_kills();
    for (int i = 0; i < GALAXY_SYSTEMS; i++) discovered[i] = data_sold[i] = 0;
}

static const char *place_name(int sys, int nav) {
    const StarSystem *s = galaxy_sys(sys);
    return nav == NAV_STATION ? s->station_name : s->planet_name;
}

static int giver_for(int sys) {
    switch (galaxy_sys(sys)->owner) {
    case OWN_FEDERATION: return GIVER_UFS;
    case OWN_REBEL:      return GIVER_FSA;
    case OWN_SYNDICATE:  return GIVER_SYN;
    default:             return GIVER_IND;
    }
}

/* Somewhere else to deliver to: any other dock, or (planets_only) a colony. */
static void pick_dest(Mission *m, int sys, int nav, int planets_only) {
    for (int tries = 0; tries < 20; tries++) {
        int s = rng_range(4) ? galaxy_random_near(sys, 3, 1) : sys;      /* within 3 jumps (M16a) */
        if (s < 0) s = sys;
        int n = planets_only ? NAV_PLANET : rng_range(2) ? NAV_PLANET : NAV_STATION;
        if (s == sys && n == nav) continue;
        if (galaxy_sys(s)->owner == OWN_SYNDICATE && m->giver != GIVER_SYN && tries < 10) continue;
        m->dest_sys = (u8) s; m->dest_nav = (u8) n;
        return;
    }
    m->dest_sys = (u8) sys; m->dest_nav = (u8) (nav == NAV_STATION ? NAV_PLANET : NAV_STATION);
}

/* A system within reach that has pirates: for bounties and intercepts. */
static int pirate_country(int sys) {
    for (int t = 0; t < 30; t++) {
        int s = galaxy_random_near(sys, 4, 0);
        if (s >= 0 && galaxy_sys(s)->pirates > 0) return s;
    }
    return sys;
}

/* Game hours along the whole route (M16a: the galaxy has many hops now). */
static int hours_to(int from, int to) {
    if (from == to) return 2;
    int h = 0, at = from, guard = GALAXY_SYSTEMS;
    while (at != to && guard-- > 0) {
        int n = galaxy_next_hop(at, to);
        if (n < 0) return galaxy_jump_hours(from, to);
        h += galaxy_jump_hours(at, n);
        at = n;
    }
    return h;
}

static void make_typed(Mission *m, int t, int sys, int nav, int syndicate);
static int carries(const Mission *m);

static void make_offer(Mission *m, int sys, int nav, int syndicate) {
    int t;
    if (syndicate == BOARD_LOUNGE) {
        static const u8 lounge_types[4] = { MI_CARGO, MI_PASSENGER, MI_COURIER, MI_SUPPLY };
        t = lounge_types[rng_range(4)];
        make_typed(m, t, sys, nav, 0);
        return;
    }
    if (syndicate) {
        static const u8 syn_types[5] = { MI_SMUGGLE, MI_SMUGGLE, MI_SMUGGLE, MI_COURIER, MI_PASSENGER };
        t = syn_types[rng_range(5)];
    } else {
        static const u8 guild_types[9] = { MI_CARGO, MI_CARGO, MI_COURIER, MI_SUPPLY, MI_PASSENGER,
                                           MI_BOUNTY, MI_BOUNTY, MI_INTERCEPT, MI_SURVEY };
        t = guild_types[rng_range(9)];
    }
    make_typed(m, t, sys, nav, syndicate);
}

static void make_typed(Mission *m, int t, int sys, int nav, int syndicate) {
    *m = (Mission) { 0 };
    m->type = (u8) t;
    m->giver = (u8) (syndicate ? GIVER_SYN : giver_for(sys));
    int now = now_hours();
    int lvl = syn_level();

    switch (t) {
    case MI_CARGO: {
        pick_dest(m, sys, nav, 0);
        do m->commodity = (u8) rng_range(COMM_COUNT); while (commodities[m->commodity].contraband || m->commodity == COMM_SCRAP);
        m->units = (u8) (10 + rng_range(31));
        int h = hours_to(sys, m->dest_sys);
        m->reward = 400 + m->units * 25 + h * 60;
        m->deadline = now + h * 3 + 24;
        break;
    }
    case MI_COURIER: {
        pick_dest(m, sys, nav, 0);
        m->units = 1;
        int h = hours_to(sys, m->dest_sys);
        m->reward = 900 + h * 160 + (syndicate ? 600 : 0);
        m->deadline = now + h * 2 + 6;
        break;
    }
    case MI_SUPPLY: {
        pick_dest(m, sys, nav, 1);
        m->commodity = COMM_MEDICAL;
        m->units = (u8) (10 + rng_range(16));
        int h = hours_to(sys, m->dest_sys);
        m->reward = 1200 + m->units * 40 + h * 80;
        m->deadline = now + h * 2 + 8;
        break;
    }
    case MI_PASSENGER: {
        pick_dest(m, sys, nav, 0);
        m->units = (u8) (2 + rng_range(7));
        int h = hours_to(sys, m->dest_sys);
        m->reward = m->units * (250 + h * 30) + (syndicate ? 800 : 0);
        m->deadline = now + h * 3 + 12;
        break;
    }
    case MI_BOUNTY: {
        m->dest_sys = (u8) pirate_country(sys);
        static const u8 targets[6] = { DEF_SCRAPPER, DEF_JACKAL, DEF_JACKAL, DEF_RENEGADE, DEF_CHOPSHOP, DEF_VANGUARD };
        int band = galaxy_sys(m->dest_sys)->band;
        m->target_def = targets[(band + rng_range(3)) % 6];
        m->name = (u8) rng_range(8);
        m->reward = 2000 + (int) (ship_defs[m->target_def].price / 40);
        m->deadline = now + 96;
        break;
    }
    case MI_INTERCEPT: {
        m->dest_sys = (u8) pirate_country(sys);
        m->units = (u8) (3 + rng_range(4));
        m->reward = m->units * 650;
        m->deadline = now + 72;
        break;
    }
    case MI_SMUGGLE: {
        /* contraband to a UFS dock, the nearer the better */
        int s = 0;
        for (int t = 0; t < 40; t++) {
            int c = galaxy_random_near(sys, 2 + t / 8, 1);
            if (c >= 0 && galaxy_sys(c)->owner == OWN_FEDERATION) { s = c; break; }
        }
        m->dest_sys = (u8) s;
        m->dest_nav = (u8) rng_range(2);
        m->commodity = (u8) (rng_range(2) ? COMM_NARCOTICS : COMM_STOLEN_TECH);
        m->units = (u8) (6 + rng_range(8) + lvl * 6);
        int h = hours_to(sys, m->dest_sys);
        m->reward = 1500 + m->units * 140 + h * 80;
        m->deadline = now + h * 3 + 24;
        break;
    }
    default: {   /* survey */
        int s = sys;
        for (int t = 0; t < 30; t++) {                    /* an unexplored system nearby */
            s = galaxy_random_near(sys, 4, 0);
            if (s >= 0 && !discovered[s]) break;
        }
        if (s < 0) s = sys;
        m->dest_sys = (u8) s;
        m->reward = 1500 + hours_to(sys, s) * 120;
        m->deadline = 0;
        break;
    }
    }
}

void missions_board_make(int sys, int nav, int syndicate) {
    u32 key = (u32) (clock_day() * 64 + sys * 8 + nav * 2 + syndicate) | 0x80000000u;
    board_key = key;
    u32 saved = rng_state;
    rng_state = key * 2654435761u ^ 0x5bd1e995u;
    n_offers = 0;
    int want = syndicate == BOARD_LOUNGE ? 3 : syndicate ? (syn_rep() <= SYN_MARKED ? 0 : 2 + syn_level()) : MISSION_OFFERS;
    if (want > MISSION_OFFERS) want = MISSION_OFFERS;
    for (int i = 0; i < want; i++) {
        Mission m;
        make_offer(&m, sys, nav, syndicate);
        int gone = 0;
        for (int k = 0; k < n_taken; k++) if (taken[k] == (key ^ (u32) i)) gone = 1;
        if (gone) continue;
        offers[n_offers] = m;
        offers[n_offers].spawned = (u8) i;          /* remembers its slot on the board until accepted */
        n_offers++;
    }
    rng_state = saved;
}

int missions_offer_count(void) { return n_offers; }

/* A one-off contract from a stranger in the lounge (pay scaled by pct). */
void missions_make_custom(Mission *m, int type, int sys, int nav, int pct) {
    make_typed(m, type, sys, nav, 0);
    m->giver = GIVER_IND;
    m->reward = m->reward * pct / 100;
}

const char *missions_accept_custom(const Mission *m, int cargo_cap) {
    if (n_active >= MISSION_ACTIVE) return "YOU CAN ONLY HOLD 4 CONTRACTS AT ONCE";
    if (carries(m) && cargo_total() + m->units > cargo_cap) return "NOT ENOUGH ROOM IN YOUR HOLD";
    active[n_active] = *m;
    active[n_active].spawned = 0;
    active[n_active].progress = 0;
    n_active++;
    return "DEAL. IT'S ON YOUR CONTRACT LIST";
}

const char *missions_wanted_name(int i) { return wanted_names[i & 7]; }
const char *missions_wanted_story(int i) { return wanted_names[8 + (i & 7)]; }
int missions_story_active(int track) {
    for (int i = 0; i < n_active; i++) if (active[i].story == track + 1) return 1;
    return 0;
}
int missions_discovered(int sys) { return discovered[sys]; }
const Mission *missions_offer(int i) { return i >= 0 && i < n_offers ? &offers[i] : 0; }
int missions_active_count(void) { return n_active; }
const Mission *missions_active(int i) { return i >= 0 && i < n_active ? &active[i] : 0; }

static int carries(const Mission *m) {
    return m->type == MI_CARGO || m->type == MI_SUPPLY || m->type == MI_SMUGGLE || m->type == MI_PASSENGER ||
           m->type == MI_COURIER;
}

int missions_cargo_units(void) {
    int u = 0;
    for (int i = 0; i < n_active; i++) if (carries(&active[i])) u += active[i].units;
    return u;
}

int missions_contraband(void) {
    int u = 0;
    for (int i = 0; i < n_active; i++) if (active[i].type == MI_SMUGGLE) u += active[i].units;
    return u;
}

static void remove_active(int i) {
    for (int k = i; k < n_active - 1; k++) active[k] = active[k + 1];
    n_active--;
}

static void rep_for(const Mission *m, int delta) {
    if (m->story) { if (delta < 0) story_failed(m->story - 1); return; }   /* the story keeps its own score */
    if (m->giver == GIVER_UFS) faction_change(FAC_FEDERATION, delta);
    else if (m->giver == GIVER_FSA) faction_change(FAC_REBEL, delta);
    else if (m->giver == GIVER_SYN) syn_change(delta + (delta > 0 ? 1 : 0));
}

void missions_ship_lost(void) {
    for (int i = 0; i < n_active; i++)
        if (carries(&active[i])) { rep_for(&active[i], -3); remove_active(i); i--; }
}

void missions_seize_contraband(void) {
    for (int i = 0; i < n_active; i++)
        if (active[i].type == MI_SMUGGLE) { rep_for(&active[i], -3); remove_active(i); i--; }
}

const char *missions_accept(int i, int cargo_cap) {
    if (i < 0 || i >= n_offers) return 0;
    if (n_active >= MISSION_ACTIVE) return "YOU CAN ONLY HOLD 4 CONTRACTS AT ONCE";
    Mission *m = &offers[i];
    if (carries(m) && cargo_total() + m->units > cargo_cap) return "NOT ENOUGH ROOM IN YOUR HOLD";
    if (m->giver == GIVER_UFS && faction_rep(FAC_FEDERATION) < 0) return "THE UFS WON'T HIRE YOU: RAISE YOUR STANDING";
    if (m->giver == GIVER_FSA && faction_rep(FAC_REBEL) < 0) return "THE FSA WON'T HIRE YOU: RAISE YOUR STANDING";
    if (n_taken < 16) taken[n_taken++] = board_key ^ (u32) m->spawned;
    else { for (int k = 0; k < 15; k++) taken[k] = taken[k + 1]; taken[15] = board_key ^ (u32) m->spawned; }
    active[n_active] = *m;
    active[n_active].spawned = 0;
    active[n_active].progress = 0;
    n_active++;
    for (int k = i; k < n_offers - 1; k++) offers[k] = offers[k + 1];
    n_offers--;
    return "CONTRACT ACCEPTED";
}

void missions_abandon(int i) {
    if (i < 0 || i >= n_active) return;
    rep_for(&active[i], -3);
    remove_active(i);
}

void missions_describe(const Mission *m, char *title, char *detail, int len) {
    char due[24] = "NO DEADLINE";
    if (m->deadline) {
        int left = m->deadline - now_hours();
        if (left < 0) left = 0;
        snprintf(due, sizeof due, "DUE IN %d H", left);
    }
    const char *dest = place_name(m->dest_sys, m->dest_nav);
    switch (m->type) {
    case MI_CARGO: case MI_SUPPLY: case MI_SMUGGLE:
        snprintf(title, len, "%s: %d %s TO %s", type_names[m->type], m->units, commodities[m->commodity].name, dest);
        break;
    case MI_COURIER:
        snprintf(title, len, "COURIER: A SEALED PACKAGE TO %s", dest);
        break;
    case MI_PASSENGER:
        snprintf(title, len, "PASSENGERS: %d TO %s", m->units, dest);
        break;
    case MI_BOUNTY:
        snprintf(title, len, "BOUNTY: %s, IN %s", wanted_names[m->name], galaxy_sys(m->dest_sys)->name);
        break;
    case MI_INTERCEPT:
        snprintf(title, len, "INTERCEPT: DESTROY %d HOSTILES IN %s", m->units, galaxy_sys(m->dest_sys)->name);
        break;
    case MI_CAPTURE:
        snprintf(title, len, "CAPTURE: TAKE %s", galaxy_sys(m->dest_sys)->name);
        break;
    case MI_CLAIM:
        snprintf(title, len, "CLAIM: A VEYR HULL FOR YOUR FLEET");
        break;
    default:
        snprintf(title, len, "SURVEY: FIND THE ANOMALY IN %s", galaxy_sys(m->dest_sys)->name);
        break;
    }
    if (m->type == MI_BOUNTY)
        snprintf(detail, len, "PAY %d  %s  FLIES A %s  FOR %s", (int) m->reward, due,
                 ship_looks[m->target_def].name, giver_names[m->giver]);
    else if (m->type == MI_INTERCEPT && m->progress)
        snprintf(detail, len, "PAY %d  %s  %d OF %d DONE  FOR %s", (int) m->reward, due, m->progress, m->units,
                 giver_names[m->giver]);
    else
        snprintf(detail, len, "PAY %d  %s  FOR %s%s", (int) m->reward, due, giver_names[m->giver],
                 carries(m) ? "  (USES HOLD SPACE)" : "");
}

static const char *pay(int i, const char *what) {
    Mission *m = &active[i];
    credits += m->reward;
    audio_sfx(SFX_CASH);
    if (m->story) {                                   /* a story step: the story takes it from here */
        int t = m->story - 1;
        remove_active(i);
        return story_completed(t);
    }
    rep_for(m, 3);
    snprintf(msg, sizeof msg, "%s COMPLETE: PAID %d", what, (int) m->reward);
    remove_active(i);
    return msg;
}

const char *missions_on_dock(int sys, int nav) {
    const char *r = 0;
    for (int i = 0; i < n_active; i++) {
        Mission *m = &active[i];
        if (carries(m) && m->dest_sys == sys && m->dest_nav == nav) { r = pay(i, type_names[m->type]); i--; }
    }
    return r;
}

const char *missions_bounty_name(void) { return bounty_name_now ? bounty_name_now : "WANTED"; }

static int anomaly_dist(const Ship *pl, int sys) {
    return (int) approx_len((int) FROM_FP(pl->x) - anomaly_xy[sys][0], (int) FROM_FP(pl->y) - anomaly_xy[sys][1]);
}

const char *missions_update(const Ship *pl, int sys) {
    const char *r = 0;

    /* Deadlines, checked once a game hour. */
    int h = now_hours();
    if (h != last_hour) {
        last_hour = h;
        for (int i = 0; i < n_active; i++) {
            if (active[i].deadline && h > active[i].deadline) {
                snprintf(msg, sizeof msg, "CONTRACT FAILED: %s, OUT OF TIME", type_names[active[i].type]);
                rep_for(&active[i], -3);
                remove_active(i);
                i--;
                r = msg;
            }
        }
    }

    /* A discovery. A survey contract pays when you reach the anomaly, found before or not. */
    if (anomaly_dist(pl, sys) < ANOMALY_RANGE) {
        if (!discovered[sys]) {
            discovered[sys] = 1;
            snprintf(msg, sizeof msg, "DISCOVERY: %s! DATA LOGGED", anomaly_name[sys]);
            r = msg;
        }
        for (int i = 0; i < n_active; i++)
            if (active[i].type == MI_SURVEY && active[i].dest_sys == sys) { r = pay(i, "SURVEY"); break; }
    }

    /* Story objectives (M18a): a system that is yours now; a Veyr hull in your fleet. */
    for (int i = 0; i < n_active; i++) {
        Mission *m = &active[i];
        int done = 0;
        if (m->type == MI_CAPTURE && galaxy_sys(m->dest_sys)->owner == OWN_PLAYER) done = 1;
        if (m->type == MI_CLAIM)
            for (int f = 0; f < fleet_count; f++) if (fleet[f] >= DEF_SKITTER && fleet[f] <= DEF_LEVIATHAN) done = 1;
        if (done) { r = pay(i, type_names[m->type]); i--; }
    }

    /* Pirates the player destroyed (for interceptions). */
    int k = enemies_player_pirate_kills();
    int dk = k - last_kills;
    last_kills = k;
    for (int i = 0; i < n_active && dk > 0; i++) {
        Mission *m = &active[i];
        if (m->type != MI_INTERCEPT || m->dest_sys != sys) continue;
        m->progress = (u8) (m->progress + dk);
        if (m->progress >= m->units) { r = pay(i, "INTERCEPT"); i--; }
        else { snprintf(msg, sizeof msg, "INTERCEPT: %d OF %d PIRATES", m->progress, m->units); r = msg; }
    }

    /* Bounties: the wanted pilot shows up in his system; get him and you're paid. */
    bounty_name_now = 0;
    for (int i = 0; i < n_active; i++) {
        Mission *m = &active[i];
        if (m->type != MI_BOUNTY) continue;
        if (m->dest_sys != sys) { m->spawned = 0; continue; }
        if (!m->spawned) {
            if (pl->docked || enemies_bounty_slot() >= 0) continue;
            enemies_spawn_bounty(m->target_def, 2, pl->x, pl->y);
            m->spawned = 1;
            snprintf(msg, sizeof msg, "%s IS HERE, WITH TWO WINGMEN", wanted_names[m->name]);
            r = msg;
        }
        bounty_name_now = wanted_names[m->name];
        if (enemies_take_bounty_hit()) { bounty_name_now = 0; r = pay(i, "BOUNTY"); break; }
        if (enemies_bounty_slot() < 0) m->spawned = 0;            /* lost him: he'll be back */
    }
    return r;
}

const char *missions_hud(const Ship *pl, int sys) {
    for (int i = 0; i < n_active; i++) {
        const Mission *m = &active[i];
        if (m->type == MI_SURVEY && m->dest_sys == sys) {
            int dx = anomaly_xy[sys][0] - (int) FROM_FP(pl->x), dy = anomaly_xy[sys][1] - (int) FROM_FP(pl->y);
            static const char *dirs[8] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
            int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
            int d;
            if (ay > 2 * ax) d = dy < 0 ? 0 : 4;
            else if (ax > 2 * ay) d = dx > 0 ? 2 : 6;
            else d = dx > 0 ? (dy < 0 ? 1 : 3) : (dy < 0 ? 7 : 5);
            snprintf(msg, sizeof msg, "SURVEY SIGNAL: %s, %d", dirs[d], (int) approx_len(dx, dy));
            return msg;
        }
        if (m->type == MI_INTERCEPT && m->dest_sys == sys) {
            snprintf(msg, sizeof msg, "INTERCEPT: %d OF %d PIRATES", m->progress, m->units);
            return msg;
        }
    }
    return 0;
}

u8 *missions_ser(u8 *p, int load) {
    SER(p, load, active); SER(p, load, n_active); SER(p, load, taken); SER(p, load, n_taken);
    SER(p, load, discovered); SER(p, load, data_sold);
    if (load && ser_version < 11) for (int i = 0; i < MISSION_ACTIVE; i++) active[i].story = 0;   /* was padding */
    if (load) {
        for (int i = 0; i < n_active; i++) active[i].spawned = 0;   /* the wanted pilot comes back */
        last_kills = enemies_player_pirate_kills();
        last_hour = -1;
        n_offers = 0;
        board_key = 0;
    }
    return p;
}

int missions_discoveries_unsold(void) {
    int n = 0;
    for (int i = 0; i < GALAXY_SYSTEMS; i++) n += discovered[i] && !data_sold[i];
    return n;
}

int missions_sell_data(void) {
    int paid = 0;
    for (int i = 0; i < GALAXY_SYSTEMS; i++)
        if (discovered[i] && !data_sold[i]) { data_sold[i] = 1; paid += DATA_PRICE; }
    credits += paid;
    return paid;
}

/* The anomaly: a slow pulse of cyan, visible once it's on screen. */
void missions_draw_anomaly(s32 cam_x, s32 cam_y, int sys, int frame) {
    int sx = SCREEN_W / 2 + anomaly_xy[sys][0] - (int) FROM_FP(cam_x);
    int sy = SCREEN_H / 2 + anomaly_xy[sys][1] - (int) FROM_FP(cam_y);
    if (sx < -20 || sx > SCREEN_W + 20 || sy < -20 || sy > SCREEN_H + 20) return;
    int p = (frame >> 2) & 15;
    int g = p < 8 ? p : 15 - p;
    int c = discovered[sys] ? 90 : 140 + g * 14;
    gfx_rect(sx - 3, sy - 3, 6, 6, LAYER_OBJECTS, 40, c, c);
    gfx_rect(sx - 9 - g, sy, 4, 1, LAYER_OBJECTS, 30, c / 2, c / 2);
    gfx_rect(sx + 5 + g, sy, 4, 1, LAYER_OBJECTS, 30, c / 2, c / 2);
    gfx_rect(sx, sy - 9 - g, 1, 4, LAYER_OBJECTS, 30, c / 2, c / 2);
    gfx_rect(sx, sy + 5 + g, 1, 4, LAYER_OBJECTS, 30, c / 2, c / 2);
}

#ifdef FRONTIER_DEV
void missions_dev_finish(void) {
    while (n_active > 0) pay(0, type_names[active[0].type]);
}
#endif
