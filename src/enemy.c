#include <stdlib.h>
#include <stdio.h>
#include <psxgpu.h>
#include <psxgte.h>

#include "collide.h"
#include "enemy.h"
#include "faction.h"
#include "galaxy.h"
#include "outfit.h"
#include "gfx.h"
#include "rng.h"
#include "weapon.h"
#include "escort.h"
#include "veyr.h"
#include "save.h"

#define PATROL_RESPAWN    900
#define PIRATE_RESPAWN    300    /* frames until a destroyed pirate returns */
#define MERCHANT_RESPAWN  600
#define AGGRO_RANGE       520    /* pixels: pirates notice targets inside this */
#define FIRE_RANGE        300    /* pixels: opens fire inside this */
#define CLOSE_RANGE        70    /* pixels: too close, break away */
#define FLEE_HULL_PCT      40    /* pirates run below this hull percentage */
#define FLEE_FRAMES       240
#define EVADE_FRAMES       50
#define RETARGET_FRAMES    30
#define THREAT_RANGE      400    /* merchants run from pirates inside this */
#define WAYPOINT_REACHED  120

enum { E_UNUSED = 0, E_ALIVE, E_DEAD };
enum { AI_IDLE = 0, AI_ATTACK, AI_EVADE, AI_FLEE };
enum { TARGET_NONE = -2, TARGET_PLAYER = -1, TARGET_ESCORT = 64 };   /* otherwise an NPC slot; 64+k = your escort k (M15b) */

typedef struct {
    Ship ship;
    u8   used;       /* E_UNUSED / E_ALIVE / E_DEAD */
    u8   role;
    u8   ai;
    s8   target;     /* pirates: TARGET_NONE, TARGET_PLAYER or a slot */
    u8   faction;    /* patrols: FAC_* */
    u8   looted;     /* the wreck has been stripped */
    s16  timer;      /* respawn countdown (dead) or state countdown (evade, flee) */
    s16  retarget;   /* frames until the pirate looks for a new target */
    s32  wx, wy;     /* merchants: FP12 waypoint */
    s8   leader;     /* pirates in a gang: the leader's slot, or -1 (the leader, or a loner) */
    u8   gang;       /* 1 = part of a raiding gang (normal mode), 0 = stress-test loner */
    u8   wing;       /* wingman number, for its place in the formation */
    u8   provoked;   /* pirates: the player shot at it (a Syndicate friend is fair game then) */
    u8   bounty;     /* a wanted pilot from a bounty contract (M13j) */
    u8   alien;      /* a Veyr ship (M17a): no Syndicate ties, hostile to everyone */
} Enemy;

static Enemy enemies[MAX_ENEMIES];
static int slot_of(const Enemy *e) { return (int) (e - enemies); }
static int normal_pirates = PIRATE_NORMAL_COUNT, normal_merchants = MAX_MERCHANTS;
static int normal_patrols, patrol_faction = -1;
static int pirate_target = PIRATE_NORMAL_COUNT;
static int peace_mode, current_stress;
static int kills, disables, merchant_losses;
static int pirates_alive_count, merchants_alive_count;

/* Patrol scans (Milestone 13d): scan.c runs the scan; these are its levers. */
static int wanted;                 /* ran from a scan: this system's patrols hunt the player */
static int siege;                  /* M14g: a colony assault is on */
static int follow_slot = -1, follow_mode, follow_phase;

static int hostile_fac(int fac) { return faction_hostile(fac) || (wanted && fac == patrol_faction); }

void enemies_set_wanted(int w) { wanted = w; }
int  enemies_wanted(void)      { return wanted; }
void enemies_patrol_follow(int slot, int mode) { follow_slot = slot; follow_mode = mode; }

/* The three pirate types, by slot: Raider, Wayfarer, Hauler. */
static const u8 pirate_defs[3] = { DEF_RAIDER, DEF_RAIDER, DEF_FRONTIER_RUNNER };
static const u8 merchant_defs[4] = { DEF_HAULER, DEF_FRONTIER_RUNNER, DEF_ROCKBREAKER, DEF_RECLAIMER };
/* Patrol groups: the first patrol slot flies a corvette, the rest fighters. */
static const u8 ufs_patrol_defs[MAX_PATROLS] = { DEF_SENTINEL, DEF_TALON, DEF_VANGUARD, DEF_TALON };
static const u8 fsa_patrol_defs[MAX_PATROLS] = { DEF_RENEGADE, DEF_WASP, DEF_HAMMER, DEF_WASP };

/* ------------------------------------------------------------------------ */
/* Placement                                                                 */
/* ------------------------------------------------------------------------ */

static void ring_position(Ship *s, s32 px, s32 py, int min_r, int max_r) {
    int r = min_r + rng_range(max_r - min_r + 1);
    int a = rng_next() & ANGLE_MASK;
    s->x = px + isin(a) * r;
    s->y = py - icos(a) * r;
    s->angle = rng_next() & ANGLE_MASK;
}

static void merchant_new_waypoint(Enemy *e) {
    int r = 500 + rng_range(301);
    int a = rng_next() & ANGLE_MASK;
    e->wx = e->ship.x + isin(a) * r;
    e->wy = e->ship.y - icos(a) * r;
}

/* ------------------------------------------------------------------------ */
/* Pirate gangs (Milestone 13e)                                              */
/* ------------------------------------------------------------------------ */

/* A raid is a gang: a leader (first) and wingmen. Out toward the Rim the gangs
 * get bigger and fly stolen military hulls; in the Core a lone raider is all
 * that gets through. The system's pirate count caps the gang's size. */
typedef struct { u8 n; u8 defs[MAX_PIRATES]; } Roster;

/* Mostly the Syndicate's own scrap-builds (M13g), with the odd stolen military hull. */
static const Roster rosters_core[] = {
    { 1, { DEF_SCRAPPER } },
    { 2, { DEF_RAIDER, DEF_SCRAPPER } },
};
static const Roster rosters_mid[] = {
    { 3, { DEF_JACKAL, DEF_SCRAPPER, DEF_RAIDER } },
    { 3, { DEF_WAYFINDER, DEF_SCRAPPER, DEF_SCRAPPER } },
    { 3, { DEF_SCRAPPER, DEF_SCRAPPER, DEF_RAIDER } },
};
static const Roster rosters_frontier[] = {
    { 4, { DEF_JACKAL, DEF_SCRAPPER, DEF_SCRAPPER, DEF_RAIDER } },
    { 3, { DEF_CHOPSHOP, DEF_SCRAPPER, DEF_RAIDER } },
    { 4, { DEF_VANGUARD, DEF_JACKAL, DEF_SCRAPPER, DEF_SCRAPPER } },
};
static const Roster rosters_rim[] = {
    { 6, { DEF_MARAUDER, DEF_CHOPSHOP, DEF_JACKAL, DEF_SCRAPPER, DEF_SCRAPPER, DEF_RAIDER } },
    { 5, { DEF_CHOPSHOP, DEF_JACKAL, DEF_JACKAL, DEF_SCRAPPER, DEF_SCRAPPER } },
    { 5, { DEF_DEFIANT, DEF_JACKAL, DEF_SCRAPPER, DEF_SCRAPPER, DEF_RAIDER } },
};

/* Frames between raids, by security (very low .. high). Placeholders. */
/* How long between pirate raids, by the system's pirate activity 0..6 (M16c). The
 * UFS heartland sees a lone raider every 8 minutes or so; empty space a gang a minute. */
static const u16 raid_frames[7] = { 0, 28800, 18000, 12600, 9000, 5400, 3600 };
static int raid_wait(void) {
    int a = galaxy_sys(galaxy_current())->pirates;
    if (a > 6) a = 6;
    int f = raid_frames[a];
    return f + rng_range(f / 2 + 1) - f / 4;              /* +-25%: not like clockwork */
}

static int gang_timer, gang_leader = -1;
static int bounty_total;
static const char *event_msg;
static char event_buf[48];


/* Veyr swarms (M17a): Skitters and Mantises, Reavers in the bigger ones. */
static const Roster rosters_veyr[] = {
    { 2, { DEF_SKITTER, DEF_SKITTER } },
    { 3, { DEF_MANTIS, DEF_SKITTER, DEF_SKITTER } },
    { 3, { DEF_REAVER, DEF_MANTIS, DEF_MANTIS } },
    { 4, { DEF_REAVER, DEF_REAVER, DEF_MANTIS, DEF_SKITTER } },
};
static int veyr_pct, gang_alien;
void enemies_set_veyr(int pct) { veyr_pct = pct; }
int  enemies_is_alien(int slot) { return slot >= 0 && slot < MAX_ENEMIES && enemies[slot].used == E_ALIVE && enemies[slot].alien; }

static const Roster *pick_roster(void) {
    gang_alien = rng_range(100) < veyr_pct;
    if (gang_alien) {
        int d = game_difficulty % 3, k = rng_range(3) + (d == 2 ? 1 : 0) - (d == 0 && rng_range(2) ? 1 : 0);
        return &rosters_veyr[k < 0 ? 0 : k > 3 ? 3 : k];
    }
    switch (galaxy_sys(galaxy_current())->band) {
    case BAND_CORE:     return &rosters_core[rng_range(2)];
    case BAND_MID:      return &rosters_mid[rng_range(3)];
    case BAND_FRONTIER: return &rosters_frontier[rng_range(3)];
    default:            return &rosters_rim[rng_range(3)];
    }
}

const char *enemies_take_message(void) { const char *m = event_msg; event_msg = 0; return m; }
int enemies_bounties(void) { return bounty_total; }

static void enemy_place(Enemy *e, int slot, s32 px, s32 py, int stress) {
    e->alien = 0;
    if (slot < MAX_PIRATES) {
        ship_init(&e->ship, pirate_defs[slot % 3], TEAM_ENEMY);
        ring_position(&e->ship, px, py, stress ? 150 : 300, stress ? 260 : 450);
        e->role = ROLE_PIRATE;
    } else if (slot >= MAX_PIRATES + MAX_MERCHANTS) {
        int k = (slot - MAX_PIRATES - MAX_MERCHANTS) % MAX_PATROLS;
        ship_init(&e->ship, patrol_faction == FAC_REBEL ? fsa_patrol_defs[k] : ufs_patrol_defs[k],
                  patrol_faction == FAC_REBEL ? TEAM_REBEL : TEAM_FED);
        ring_position(&e->ship, px, py, stress ? 200 : 350, stress ? 300 : 600);
        e->role = ROLE_PATROL;
        e->faction = (u8) patrol_faction;
        merchant_new_waypoint(e);
    } else {
        ship_init(&e->ship, merchant_defs[rng_range(4)], TEAM_MERCHANT);
        ring_position(&e->ship, px, py, stress ? 150 : 250, stress ? 260 : 450);
        e->role = ROLE_MERCHANT;
        merchant_new_waypoint(e);
    }
    e->looted = 0;
    e->leader = -1;
    e->provoked = 0;
    e->bounty = 0;
    e->gang = 0;
    e->wing = 0;
    e->used = E_ALIVE;
    e->ai = AI_IDLE;
    e->target = TARGET_NONE;
    e->timer = 0;
    e->retarget = (s16) rng_range(RETARGET_FRAMES);
}

void enemies_init(void) {
    for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].used = E_UNUSED;
    kills = disables = merchant_losses = 0;
    pirates_alive_count = merchants_alive_count = 0;
    pirate_target = normal_pirates;
    peace_mode = 0;
    current_stress = 0;
}

void enemies_set_mode(int stress, s32 px, s32 py) {
    current_stress = stress;
    siege = 0;
    pirate_target = peace_mode ? 0 : (stress ? MAX_PIRATES : normal_pirates);
    for (int i = 0; i < MAX_ENEMIES; i++) {
        int wanted;
        if (i < MAX_PIRATES)                        wanted = stress && i < pirate_target;
        else if (i < MAX_PIRATES + MAX_MERCHANTS)   wanted = stress || (i - MAX_PIRATES) < normal_merchants;
        else                                        wanted = !stress && (i - MAX_PIRATES - MAX_MERCHANTS) < normal_patrols;
        if (wanted) enemy_place(&enemies[i], i, px, py, stress);
        else        enemies[i].used = E_UNUSED;
    }
    gang_leader = -1;
    gang_timer = raid_wait() / 3;                         /* a little quiet on arrival */
}

/* A new gang comes in from beyond the scanner's edge, already heading for you. */
static void gang_spawn(s32 px, s32 py) {
    const Roster *r = pick_roster();
    int n = r->n < pirate_target ? r->n : pirate_target;
    int a = rng_next() & ANGLE_MASK;
    int dist = 650 + rng_range(100);
    s32 lx = px + isin(a) * dist, ly = py - icos(a) * dist;
    int face = (a + ANGLE_FULL / 2) & ANGLE_MASK;        /* toward the player */
    int placed = 0;
    gang_leader = -1;
    for (int i = 0; i < MAX_PIRATES && placed < n; i++) {
        Enemy *e = &enemies[i];
        if (e->used == E_ALIVE) continue;                 /* a wreck still there */
        enemy_place(e, i, px, py, 0);
        ship_init(&e->ship, r->defs[placed], TEAM_ENEMY);
        int side = (placed & 1) ? 1 : -1, row = (placed + 1) / 2;
        s32 ox = side * row * 70, oy = row * 70;           /* a V behind the leader */
        s32 sn = isin(face), cs = icos(face);
        e->ship.x = lx + ox * cs - oy * sn;
        e->ship.y = ly + ox * sn + oy * cs;
        e->ship.angle = face;
        e->gang = 1;
        e->alien = (u8) gang_alien;
        e->wing = (u8) placed;
        if (placed == 0) gang_leader = i;
        e->leader = (s8) (placed == 0 ? -1 : gang_leader);
        placed++;
    }
    if (placed == 0) return;
    if (gang_alien)
        snprintf(event_buf, sizeof event_buf, "VEYR SWARM ON SCANNERS: %d SHIPS. EVERYONE RUN", placed);
    else if (syn_friendly())
        snprintf(event_buf, sizeof event_buf, "SYNDICATE CREW ON SCANNERS: %d SHIP%s. THEY KNOW YOU", placed,
                 placed == 1 ? "" : "S");
    else if (placed == 1) snprintf(event_buf, sizeof event_buf, "PIRATE ON SCANNERS: A LONE %s", ship_looks[r->defs[0]].name);
    else snprintf(event_buf, sizeof event_buf, "PIRATE GANG ON SCANNERS: %d SHIPS, LED BY A %s", placed,
                  ship_looks[r->defs[0]].name);
    event_msg = event_buf;
}

void enemies_dev_gang(s32 px, s32 py) { gang_spawn(px, py); }

/* ------------------------------------------------------------------------ */
/* Planet conquest (M14g)                                                    */
/* ------------------------------------------------------------------------ */

int enemies_siege(void) { return siege; }

void enemies_siege_wave(const u8 *defs, int n, s32 bx, s32 by, int team, s32 px, s32 py) {
    if (!siege)                                       /* the first wave: traffic clears out */
        for (int i = MAX_PIRATES; i < MAX_ENEMIES; i++) enemies[i].used = E_UNUSED;
    siege = 1;
    int a = rng_next() & ANGLE_MASK;                  /* scatter the launch a little round the base */
    int face = 0;
    {
        int dx = (int) FROM_FP(px - bx), dy = (int) FROM_FP(py - by);   /* toward the player */
        face = (dx >= 0 ? ANGLE_FULL / 4 : ANGLE_FULL * 3 / 4);
        if (dy < 0 && (dx < 0 ? -dx : dx) < (-dy)) face = 0;
        if (dy > 0 && (dx < 0 ? -dx : dx) < dy) face = ANGLE_FULL / 2;
    }
    int placed = 0;
    for (int pass = 0; pass < 2; pass++)
    for (int i = MAX_PIRATES; i < MAX_ENEMIES && placed < n; i++) {
        Enemy *e = &enemies[i];
        if (e->used == E_ALIVE && (pass == 0 || !ship_disabled(&e->ship))) continue;   /* wrecks go only if short of room */
        ship_init(&e->ship, defs[placed], team);
        int r = 60 + placed * 30, b = (a + placed * (ANGLE_FULL / 6)) & ANGLE_MASK;
        e->ship.x = bx + isin(b) * r;
        e->ship.y = by - icos(b) * r;
        e->ship.angle = face;
        e->role = ROLE_DEFENDER;
        e->faction = (u8) (team == TEAM_REBEL ? FAC_REBEL : FAC_FEDERATION);
        e->looted = 0; e->leader = -1; e->provoked = 1; e->bounty = 0;
        e->alien = (u8) (defs[placed] >= DEF_SKITTER && defs[placed] <= DEF_LEVIATHAN);
        e->gang = 0; e->wing = (u8) placed;
        e->used = E_ALIVE;
        e->ai = AI_IDLE;
        e->target = TARGET_PLAYER;
        e->timer = 0;
        e->retarget = (s16) rng_range(RETARGET_FRAMES);
        placed++;
    }
    if (follow_slot >= MAX_PIRATES) follow_slot = -1;
}

int enemies_defenders_left(void) {
    int n = 0;
    for (int i = MAX_PIRATES; i < MAX_ENEMIES; i++)
        if (enemies[i].used == E_ALIVE && enemies[i].role == ROLE_DEFENDER && !ship_disabled(&enemies[i].ship)) n++;
    return n;
}

void enemies_siege_end(void) {
    siege = 0;
    for (int i = MAX_PIRATES; i < MAX_ENEMIES; i++) {
        Enemy *e = &enemies[i];
        if (e->used != E_UNUSED) continue;
        int want = i < MAX_PIRATES + MAX_MERCHANTS ? (i - MAX_PIRATES) < normal_merchants
                                                   : (i - MAX_PIRATES - MAX_MERCHANTS) < normal_patrols;
        if (want) { e->used = E_DEAD; e->timer = MERCHANT_RESPAWN; }
    }
}

/* Missions (M13j). */
static int player_pirate_kills, bounty_hit;
int enemies_player_pirate_kills(void) { return player_pirate_kills; }
int enemies_take_bounty_hit(void) { int h = bounty_hit; bounty_hit = 0; return h; }
int enemies_bounty_slot(void) {
    for (int i = 0; i < MAX_PIRATES; i++)
        if (enemies[i].used == E_ALIVE && enemies[i].bounty) return i;
    return -1;
}
int enemies_is_bounty(int slot) { return slot >= 0 && slot < MAX_ENEMIES && enemies[slot].used == E_ALIVE && enemies[slot].bounty; }

/* A wanted pilot and his wingmen arrive, heading for the player. Always hostile. */
void enemies_spawn_bounty(int def, int wingmen, s32 px, s32 py) {
    int a = rng_next() & ANGLE_MASK;
    s32 lx = px + isin(a) * 700, ly = py - icos(a) * 700;
    int face = (a + ANGLE_FULL / 2) & ANGLE_MASK;
    int placed = 0, lead = -1;
    for (int i = 0; i < MAX_PIRATES && placed <= wingmen; i++) {
        Enemy *e = &enemies[i];
        if (e->used == E_ALIVE) continue;
        enemy_place(e, i, px, py, 0);
        ship_init(&e->ship, placed == 0 ? def : DEF_SCRAPPER, TEAM_ENEMY);
        int side = (placed & 1) ? 1 : -1, row = (placed + 1) / 2;
        s32 ox = side * row * 80, oy = row * 80;
        s32 sn = isin(face), cs = icos(face);
        e->ship.x = lx + ox * cs - oy * sn;
        e->ship.y = ly + ox * sn + oy * cs;
        e->ship.angle = face;
        e->gang = 1;
        e->provoked = 1;                           /* a wanted man doesn't care who your friends are */
        e->wing = (u8) placed;
        if (placed == 0) { lead = i; e->bounty = 1; }
        e->leader = (s8) (placed == 0 ? -1 : lead);
        placed++;
    }
}

void enemies_dev_spawn(int def, int wreck, s32 px, s32 py) {
    int slot = 0;
    for (int i = 0; i < MAX_PIRATES; i++) if (enemies[i].used != E_ALIVE) { slot = i; break; }
    Enemy *e = &enemies[slot];
    if (slot == gang_leader) gang_leader = -1;
    enemy_place(e, slot, px, py, 0);
    ship_init(&e->ship, def, TEAM_ENEMY);
    int a = rng_next() & ANGLE_MASK;
    e->ship.x = px + isin(a) * 260;
    e->ship.y = py - icos(a) * 260;
    e->ship.angle = (a + ANGLE_FULL / 2) & ANGLE_MASK;
    e->gang = 1;                                   /* no respawn when it's gone */
    if (wreck) {
        e->ship.sys[SYS_ENGINE] = 0;
        e->ship.hull = e->ship.def.hull_max / 8;
        for (int k = 0; k < ARC_COUNT; k++) e->ship.shield[k] = 0;
    }
}

/* The leader is down: about half the gang loses its nerve and runs. */
static void gang_scatter(void) {
    int ran = 0;
    for (int i = 0; i < MAX_PIRATES; i++) {
        Enemy *e = &enemies[i];
        if (e->used != E_ALIVE || e->leader != gang_leader) continue;
        e->leader = -1;
        if (ship_disabled(&e->ship)) continue;
        if (rng_range(2)) { e->ai = AI_FLEE; e->timer = FLEE_FRAMES * 3; ran++; }
    }
    gang_leader = -1;
    event_msg = ran ? "THE PIRATE LEADER IS DOWN: THE GANG SCATTERS" : "THE PIRATE LEADER IS DOWN";
}

void enemies_set_system(int pirates, int merchants, int patrols, int faction) {
    normal_patrols = patrols > MAX_PATROLS ? MAX_PATROLS : patrols;
    patrol_faction = faction;
    wanted = 0;                      /* a new system: a scan you ran from stays behind */
    follow_slot = -1;
    normal_pirates = pirates > MAX_PIRATES ? MAX_PIRATES : pirates;
    normal_merchants = merchants > MAX_MERCHANTS ? MAX_MERCHANTS : merchants;
}

void enemies_set_peace(int peace, s32 px, s32 py) {
    peace_mode = peace;
    enemies_set_mode(current_stress, px, py);
}

int enemies_peace(void) { return peace_mode; }

int enemies_pirates_alive(void)    { return pirates_alive_count; }
int enemies_pirates_target(void)   { return pirate_target; }
int enemies_merchants_alive(void)  { return merchants_alive_count; }
int enemies_kills(void)            { return kills; }
int enemies_disables(void)         { return disables; }
int enemies_merchant_losses(void)  { return merchant_losses; }

/* ------------------------------------------------------------------------ */
/* Looking at NPCs                                                           */
/* ------------------------------------------------------------------------ */

const Ship *enemies_ship(int slot) {
    if (slot < 0 || slot >= MAX_ENEMIES) return NULL;
    return enemies[slot].used == E_ALIVE ? &enemies[slot].ship : NULL;
}

int enemies_role(int slot) {
    return (slot >= 0 && slot < MAX_ENEMIES) ? enemies[slot].role : ROLE_PIRATE;
}

static int in_range(const Enemy *e, s32 px, s32 py, int range) {
    int dx = (int) FROM_FP(e->ship.x - px);
    int dy = (int) FROM_FP(e->ship.y - py);
    return approx_len(dx, dy) <= range;
}

int enemies_nearest(int role, s32 px, s32 py, int range) {
    int best = -1, best_d = range + 1;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &enemies[i];
        if (e->used != E_ALIVE || e->role != role) continue;
        int dx = (int) FROM_FP(e->ship.x - px);
        int dy = (int) FROM_FP(e->ship.y - py);
        int d = (int) approx_len(dx, dy);
        if (d < best_d) { best_d = d; best = i; }
    }
    return best;
}

int enemies_hostiles_near(s32 px, s32 py, int range) {
    int n = 0;
    for (int i = 0; i < MAX_PIRATES; i++) {
        const Enemy *e = &enemies[i];
        if (e->used == E_ALIVE && !ship_disabled(&e->ship) && in_range(e, px, py, range))
            n++;
    }
    return n;
}

int enemies_active_hostile(int slot) {
    if (slot < 0 || slot >= MAX_ENEMIES) return 0;
    const Enemy *e = &enemies[slot];
    if (e->used != E_ALIVE || ship_disabled(&e->ship) || e->role == ROLE_MERCHANT) return 0;
    if (e->role == ROLE_PIRATE) return e->alien || !syn_friendly() || e->provoked;
    if (e->role == ROLE_DEFENDER) return 1;
    return hostile_fac(e->faction);
}

int enemies_turret_target(s32 px, s32 py, int range) {
    int best = -1, best_d = range + 1;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &enemies[i];
        if (e->used != E_ALIVE || ship_disabled(&e->ship)) continue;
        if (e->role == ROLE_MERCHANT) continue;
        if (e->role == ROLE_PATROL && !hostile_fac(e->faction)) continue;
        if (e->role == ROLE_PIRATE && !e->alien && syn_friendly() && !e->provoked) continue;
        int d = (int) approx_len(FROM_FP(e->ship.x - px), FROM_FP(e->ship.y - py));
        if (d < best_d) { best_d = d; best = i; }
    }
    return best;
}

int enemies_nearest_hostile(s32 px, s32 py, int range) {
    int best = -1, best_d = range + 1;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies_active_hostile(i)) continue;
        const Enemy *e = &enemies[i];
        int d = (int) approx_len(FROM_FP(e->ship.x - px), FROM_FP(e->ship.y - py));
        if (d < best_d) { best_d = d; best = i; }
    }
    return best;
}

int enemies_cycle(int current, s32 px, s32 py, int range) {
    for (int k = 0; k < MAX_ENEMIES; k++) {
        int i = (current + 1 + k) % MAX_ENEMIES;
        if (i < 0) i += MAX_ENEMIES;
        const Enemy *e = &enemies[i];
        if (e->used == E_ALIVE && in_range(e, px, py, range)) return i;
    }
    return -1;
}

/* ------------------------------------------------------------------------ */
/* AI                                                                        */
/* ------------------------------------------------------------------------ */

/* Turn toward a point seen at (f ahead, r to the right), both in pixels. */
static int steer(int f, int r) {
    int ar = r < 0 ? -r : r;
    if (f < 0)        return r >= 0 ? 1 : -1;   /* behind us: turn the short way round */
    if (ar * 8 > f)   return r >= 0 ? 1 : -1;   /* more than about 7 degrees off */
    return 0;
}

static void clear_controls(ShipControls *c) {
    c->turn = 0;  c->fwd = 0;  c->side = 0;
    c->boost = c->fire1 = c->fire2 = c->cruise = 0;
}

/* A docked player is safe: pirates do not see them. */
static int targetable(const Ship *pl) { return ship_alive(pl) && !pl->docked; }

static int dist_between(const Ship *a, const Ship *b) {
    return (int) approx_len((int) FROM_FP(b->x - a->x), (int) FROM_FP(b->y - a->y));
}

/* Your nearest active escort closer than *best_d (which it lowers), or -1. */
static int nearest_escort(const Enemy *e, int *best_d) {
    int best = -1;
    for (int k = 0; k < MAX_ESCORTS; k++) {
        const Ship *es = escort_ship(k);
        if (!es || ship_disabled(es)) continue;
        int d = dist_between(&e->ship, es);
        if (d < *best_d) { *best_d = d; best = k; }
    }
    return best;
}

/* Patrol: the nearest active pirate; also the player if this faction is hostile. */
static void patrol_pick_target(Enemy *e, const Ship *pl) {
    int best = TARGET_NONE;
    int best_d = AGGRO_RANGE + 150;
    for (int p = 0; p < MAX_PIRATES; p++) {
        if (enemies[p].used != E_ALIVE || ship_disabled(&enemies[p].ship)) continue;
        int d = dist_between(&e->ship, &enemies[p].ship);
        if (d < best_d) { best = p; best_d = d; }
    }
    if (hostile_fac(e->faction) && targetable(pl)) {
        int d = dist_between(&e->ship, pl) + 140 * mod_level[MOD_JAMMER];
        if (d < best_d + 200) { best = TARGET_PLAYER; best_d = d - 200; }
    }
    if (hostile_fac(e->faction)) {
        int k = nearest_escort(e, &best_d);
        if (k >= 0) best = TARGET_ESCORT + k;
    }
    e->target = (s8) best;
}

/* Fly to the waypoint, choosing a new one on arrival. */
static void travel(Enemy *e, ShipControls *c) {
    Ship *s = &e->ship;
    int dx = (int) FROM_FP(e->wx - s->x);
    int dy = (int) FROM_FP(e->wy - s->y);
    if (approx_len(dx, dy) < WAYPOINT_REACHED) { merchant_new_waypoint(e); return; }
    s32 sn = isin(s->angle), cs = icos(s->angle);
    int f = (dx * sn - dy * cs) >> FP_SHIFT;
    int r = (dx * cs + dy * sn) >> FP_SHIFT;
    c->turn = (s8) steer(f, r);
    c->fwd = FP_ONE / 2;
}

/* A patrol on a scan: hold close beside the player (mode 1) or circle him (mode 2). */
static void follow_player(Enemy *e, const Ship *pl, ShipControls *c) {
    Ship *s = &e->ship;
    s32 gx = pl->x, gy = pl->y;
    int hold = 150;
    if (follow_mode == 2) {
        int a = (follow_phase += 6) & ANGLE_MASK;
        gx += isin(a) * 230;
        gy -= icos(a) * 230;
        hold = 40;
    }
    int dx = (int) FROM_FP(gx - s->x);
    int dy = (int) FROM_FP(gy - s->y);
    int d = (int) approx_len(dx, dy);
    s32 sn = isin(s->angle), cs = icos(s->angle);
    int f = (dx * sn - dy * cs) >> FP_SHIFT;
    int r = (dx * cs + dy * sn) >> FP_SHIFT;
    if (d < hold) {                       /* there: face the player, and stop */
        dx = (int) FROM_FP(pl->x - s->x);
        dy = (int) FROM_FP(pl->y - s->y);
        if ((int) ship_speed(s) > FP_ONE / 3) {          /* still moving: thrust against it */
            int toward = (int) ((s->vx >> 4) * dx + (s->vy >> 4) * dy) > 0;
            c->fwd = toward ? -FP_ONE / 2 : FP_ONE / 2;
        }
        f = (dx * sn - dy * cs) >> FP_SHIFT;
        r = (dx * cs + dy * sn) >> FP_SHIFT;
        c->turn = (s8) steer(f, r);
        return;
    }
    c->turn = (s8) steer(f, r);
    /* Come alongside, don't overshoot (M16c): aim for a speed that falls with the
     * distance left, and brake hard when going faster than that. */
    int sp = (int) ship_speed(s), want = (int) TO_FP(d) / 70;
    if (sp > want + FP_ONE / 4) c->fwd = -FP_ONE;
    else if (sp < want) c->fwd = d > 400 ? FP_ONE : FP_ONE * 2 / 3;
}

/* A gang leader with nothing in range heads for the player. */
static void hunt(Enemy *e, const Ship *pl, ShipControls *c) {
    Ship *s = &e->ship;
    int dx = (int) FROM_FP(pl->x - s->x), dy = (int) FROM_FP(pl->y - s->y);
    s32 sn = isin(s->angle), cs = icos(s->angle);
    c->turn = (s8) steer((dx * sn - dy * cs) >> FP_SHIFT, (dx * cs + dy * sn) >> FP_SHIFT);
    c->fwd = (e->role == ROLE_PIRATE && !e->alien && syn_friendly() && approx_len(dx, dy) < 300) ? 0 : FP_ONE * 2 / 3;   /* friends just come to look */
}

/* A wingman keeps its place in the V behind its leader. */
static void form_up(Enemy *e, const Ship *ld, ShipControls *c) {
    Ship *s = &e->ship;
    int side = (e->wing & 1) ? 1 : -1, row = (e->wing + 1) / 2;
    s32 ox = side * row * 70, oy = row * 70;
    s32 lsn = isin(ld->angle), lcs = icos(ld->angle);
    s32 gx = ld->x + ox * lcs - oy * lsn, gy = ld->y + ox * lsn + oy * lcs;
    int dx = (int) FROM_FP(gx - s->x), dy = (int) FROM_FP(gy - s->y);
    int d = (int) approx_len(dx, dy);
    s32 sn = isin(s->angle), cs = icos(s->angle);
    if (d < 50) {                                   /* in place: match the leader's heading */
        int diff = (ld->angle - s->angle) & ANGLE_MASK;
        c->turn = (s8) (diff < 40 || diff > ANGLE_FULL - 40 ? 0 : diff < ANGLE_FULL / 2 ? 1 : -1);
        c->fwd = FP_ONE * 2 / 3;
        return;
    }
    c->turn = (s8) steer((dx * sn - dy * cs) >> FP_SHIFT, (dx * cs + dy * sn) >> FP_SHIFT);
    c->fwd = d > 150 ? FP_ONE : FP_ONE * 2 / 3;
}

/* Picks the nearest worthwhile target for a pirate. The player is preferred:
 * a merchant must be 60 px closer to be chosen instead. */
static void pirate_pick_target(Enemy *e, const Ship *pl) {
    int best = TARGET_NONE;
    int best_d = AGGRO_RANGE;

    if (targetable(pl) && (!syn_friendly() || e->provoked)) {   /* a Syndicate friend is left alone */
        int d = dist_between(&e->ship, pl) - 60 + 140 * mod_level[MOD_JAMMER];   /* a jammer hides you longer */
        if (d < best_d) { best = TARGET_PLAYER; best_d = d; }
    }
    for (int m = MAX_PIRATES; m < MAX_ENEMIES; m++) {
        if (enemies[m].used != E_ALIVE) continue;
        int d = dist_between(&e->ship, &enemies[m].ship);
        if (d < best_d) { best = m; best_d = d; }
    }
    if (!syn_friendly() || e->provoked) {                       /* your escorts and garrisons too (M15b) */
        int k = nearest_escort(e, &best_d);
        if (k >= 0) best = TARGET_ESCORT + k;
    }
    e->target = (s8) best;
}

static const Ship *pirate_target_ship(const Enemy *e, const Ship *pl) {
    if (e->target == TARGET_PLAYER) return targetable(pl) ? pl : NULL;
    if (e->target >= TARGET_ESCORT) {
        const Ship *es = escort_ship(e->target - TARGET_ESCORT);
        return es && !ship_disabled(es) ? es : NULL;
    }
    if (e->target >= 0 && enemies[e->target].used == E_ALIVE) return &enemies[e->target].ship;
    return NULL;
}

static void pirate_think(Enemy *e, const Ship *pl, ShipControls *c) {
    Ship *s = &e->ship;
    clear_controls(c);

    if (ship_disabled(s)) return;                 /* helpless: drifts */

    const Enemy *lead = (e->role == ROLE_PIRATE && e->leader >= 0 && enemies[e->leader].used == E_ALIVE &&
                         !ship_disabled(&enemies[e->leader].ship)) ? &enemies[e->leader] : 0;
    if (--e->retarget <= 0 || !pirate_target_ship(e, pl)) {
        if (e->role == ROLE_PATROL) patrol_pick_target(e, pl);
        else if (e->role == ROLE_DEFENDER) {                 /* the player, or an escort that's nearer */
            int bd = targetable(pl) ? dist_between(&e->ship, pl) : 100000;
            int k = nearest_escort(e, &bd);
            e->target = (s8) (k >= 0 ? TARGET_ESCORT + k : targetable(pl) ? TARGET_PLAYER : TARGET_NONE);
        }
        else if (lead && pirate_target_ship(lead, pl)) e->target = lead->target;   /* the gang fights together */
        else                        pirate_pick_target(e, pl);
        e->retarget = RETARGET_FRAMES;
    }
    const Ship *tgt = pirate_target_ship(e, pl);
    c->tgt = tgt;
    if (!tgt) {
        e->ai = AI_IDLE;
        if (e->role == ROLE_PATROL && slot_of(e) == follow_slot && targetable(pl)) follow_player(e, pl, c);
        else if (e->role == ROLE_PATROL) travel(e, c);
        else if (lead) form_up(e, &lead->ship, c);
        else if (e->role == ROLE_DEFENDER && targetable(pl)) hunt(e, pl, c);
        else if (e->gang && targetable(pl)) hunt(e, pl, c);
        return;
    }

    int dx = (int) FROM_FP(tgt->x - s->x);
    int dy = (int) FROM_FP(tgt->y - s->y);
    int dist = (int) approx_len(dx, dy);
    s32 sn = isin(s->angle);
    s32 cs = icos(s->angle);

    /* Aim where the target WILL be when a bolt arrives. A bolt inherits the
     * shooter's own velocity, so the relative velocity is what matters.
     * Flight time in frames = distance / bolt speed (8 px per frame). */
    int t = dist >> 3;
    int lx = dx + (int) (((tgt->vx - s->vx) * t) >> FP_SHIFT);
    int ly = dy + (int) (((tgt->vy - s->vy) * t) >> FP_SHIFT);
    int f = (lx * sn - ly * cs) >> FP_SHIFT;   /* aim point ahead of us */
    int r = (lx * cs + ly * sn) >> FP_SHIFT;   /* aim point to our right */

    /* --- State changes --- */
    if (e->ai == AI_EVADE || e->ai == AI_FLEE) {
        if (--e->timer <= 0) e->ai = AI_ATTACK;
    }
    if (e->ai != AI_FLEE && e->role != ROLE_DEFENDER &&      /* defenders hold to the last */
        s->hull * 100 < (int) s->def.hull_max * FLEE_HULL_PCT) {
        e->ai = AI_FLEE;
        e->timer = FLEE_FRAMES;
    }
    if (e->ai == AI_IDLE) e->ai = AI_ATTACK;
    if (e->ai == AI_ATTACK && dist < CLOSE_RANGE) {
        e->ai = AI_EVADE;
        e->timer = EVADE_FRAMES;
    }

    /* --- Actions --- */
    switch (e->ai) {
    case AI_ATTACK:
        c->turn = (s8) steer(f, r);
        /* Close in to about 200 px, then back off so they stay in a firing
         * position instead of flying past. No strafing: that would also
         * switch off the drag that settles them. */
        if (dist > 230)      c->fwd = FP_ONE;
        else if (dist < 170) c->fwd = -FP_ONE / 2;
        if (dist < FIRE_RANGE) c->fire1 = 1;     /* each gun fires only if it can bear */
        break;
    case AI_EVADE:
    case AI_FLEE:
        c->turn = (s8) steer(-f, -r);         /* point away from the target */
        c->fwd = FP_ONE;
        c->boost = 1;
        break;
    default:
        break;
    }
}

static void merchant_think(Enemy *e, ShipControls *c) {
    Ship *s = &e->ship;
    clear_controls(c);

    if (ship_disabled(s)) return;

    s32 sn = isin(s->angle);
    s32 cs = icos(s->angle);

    /* Threat: the nearest active (not disabled) pirate within range. */
    const Ship *threat = NULL;
    int threat_d = THREAT_RANGE;
    for (int i = 0; i < MAX_PIRATES; i++) {
        const Enemy *p = &enemies[i];
        if (p->used != E_ALIVE || ship_disabled(&p->ship)) continue;
        int d = dist_between(s, &p->ship);
        if (d < threat_d) { threat_d = d; threat = &p->ship; }
    }

    if (threat) {                              /* run, with the afterburner */
        int dx = (int) FROM_FP(threat->x - s->x);
        int dy = (int) FROM_FP(threat->y - s->y);
        int f = (dx * sn - dy * cs) >> FP_SHIFT;
        int r = (dx * cs + dy * sn) >> FP_SHIFT;
        c->turn = (s8) steer(-f, -r);
        c->fwd = FP_ONE;
        c->boost = 1;
        return;
    }

    /* Otherwise travel to the waypoint, and choose a new one on arrival. */
    int dx = (int) FROM_FP(e->wx - s->x);
    int dy = (int) FROM_FP(e->wy - s->y);
    if (approx_len(dx, dy) < WAYPOINT_REACHED) {
        merchant_new_waypoint(e);
        return;
    }
    int f = (dx * sn - dy * cs) >> FP_SHIFT;
    int r = (dx * cs + dy * sn) >> FP_SHIFT;
    c->turn = (s8) steer(f, r);
    c->fwd = FP_ONE;
}

void enemies_update(const Ship *player, int stress, int frame) {
    (void) frame;
    int pirate_leash   = stress ? 500 : 900;
    int merchant_leash = 1100;
    int np = 0, nm = 0;

    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &enemies[i];
        if (e->used == E_UNUSED) continue;

        if (e->used == E_DEAD) {
            if (i < MAX_PIRATES && !stress) { e->used = E_UNUSED; continue; }   /* gangs come as a whole */
            if (siege && i >= MAX_PIRATES) continue;              /* no traffic during a siege */
            if (--e->timer <= 0) enemy_place(e, i, player->x, player->y, stress);
            continue;
        }
        if (e->role == ROLE_PIRATE) np++; else if (e->role == ROLE_MERCHANT) nm++;

        ShipControls c = { 0 };
        if (e->role != ROLE_MERCHANT) pirate_think(e, player, &c);
        else                        merchant_think(e, &c);
        ship_update(&e->ship, &c);
        proj_spawn_owner = slot_of(e) + 1;     /* lets a hit name its shooter */
        ship_fire(&e->ship, &c);
        proj_spawn_owner = 0;

        /* Too far from the player: replace with a fresh ship at range. */
        int leash = (e->role == ROLE_PIRATE) ? pirate_leash : merchant_leash;
        if (e->role == ROLE_PATROL) leash = 1300;
        if (e->role == ROLE_DEFENDER) leash = 0x7fffffff;      /* they stay over their colony */
        if (dist_between(player, &e->ship) > leash) {
            if (e->role == ROLE_PIRATE && e->gang) {             /* left behind: that pirate is gone */
                if (i == gang_leader) gang_leader = -1;
                e->used = E_UNUSED;
            } else {
                enemy_place(e, i, player->x, player->y, stress);
            }
        }
    }

    /* Raids: when no pirate is left fighting, the next gang comes after a while. */
    if (!stress && pirate_target > 0 && !siege) {
        int active = 0;
        for (int i = 0; i < MAX_PIRATES; i++)
            if (enemies[i].used == E_ALIVE && !ship_disabled(&enemies[i].ship)) active++;
        if (active == 0 && --gang_timer <= 0) {
            gang_spawn(player->x, player->y);
            gang_timer = raid_wait();
#ifdef FRONTIER_HOST
            if (getenv("FR_SCANLOG")) fprintf(stderr, "[raid] gang; next in %d frames\n", gang_timer);
#endif
        }
    }
    pirates_alive_count = np;
    merchants_alive_count = nm;
}

/* ------------------------------------------------------------------------ */
/* Collision and damage                                                      */
/* ------------------------------------------------------------------------ */

void enemies_add_hittables(void) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &enemies[i];
        if (e->used != E_ALIVE) continue;
        collide_add(FROM_FP(e->ship.x), FROM_FP(e->ship.y),
                    ship_looks[e->ship.def_index].radius,
                    e->ship.team, ENEMY_ID_BASE + i);
    }
}

int enemies_wreck_near(s32 px, s32 py, int range) {
    int best = -1;
    s32 best_d = 0x7fffffff;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &enemies[i];
        if (e->used != E_ALIVE || !ship_disabled(&e->ship)) continue;
        s32 d = approx_len(FROM_FP(e->ship.x) - px, FROM_FP(e->ship.y) - py);
        if (d <= range && d < best_d) { best = i; best_d = d; }
    }
    return best;
}

int enemies_looted(int slot) { return slot >= 0 && slot < MAX_ENEMIES && enemies[slot].looted; }
void enemies_mark_looted(int slot) { if (slot >= 0 && slot < MAX_ENEMIES) enemies[slot].looted = 1; }

void enemies_remove(int slot) {
    if (slot < 0 || slot >= MAX_ENEMIES || enemies[slot].used != E_ALIVE) return;
    enemies[slot].used = E_DEAD;
    enemies[slot].timer = enemies[slot].role == ROLE_PIRATE ? PIRATE_RESPAWN
                        : enemies[slot].role == ROLE_PATROL ? PATROL_RESPAWN : MERCHANT_RESPAWN;
}

void enemies_damage(int slot, int damage, int hx, int hy) {
    if (slot < 0 || slot >= MAX_ENEMIES) return;
    Enemy *e = &enemies[slot];
    if (e->used != E_ALIVE) return;

    int was_down = ship_disabled(&e->ship);
    int result = ship_damage(&e->ship, damage, hx, hy);
    if (proj_hit_owner >= 200 && !was_down && (result == DMG_DESTROYED || result == DMG_DISABLED))
        escort_scored(proj_hit_owner - 200);          /* an escort's pilot gets the credit (M15b) */

    /* Reputation: the player shooting a patrol, or helping against pirates. */
    if (collide_shooter_team == TEAM_PLAYER && e->role == ROLE_PIRATE &&
        (result == DMG_DESTROYED || result == DMG_DISABLED)) {
        player_pirate_kills++;
        if (e->bounty) { bounty_hit = 1; e->bounty = 0; }
    }
    if (collide_shooter_team == TEAM_PLAYER && e->role == ROLE_DEFENDER && e->alien &&
        (result == DMG_DESTROYED || result == DMG_DISABLED)) veyr_on_kill();
    if (collide_shooter_team == TEAM_PLAYER && e->role == ROLE_PIRATE && e->alien) {
        if (result == DMG_DESTROYED || result == DMG_DISABLED) veyr_on_kill();      /* nav cores (M17a) */
    } else if (collide_shooter_team == TEAM_PLAYER && e->role == ROLE_PIRATE) {
        if (syn_friendly() && !e->provoked) event_msg = "YOU FIRED ON A SYNDICATE CREW!";
        e->provoked = 1;
        if (e->leader >= 0) enemies[e->leader].provoked = 1;      /* the gang takes it personally */
        for (int i = 0; i < MAX_PIRATES; i++) if (enemies[i].leader == slot) enemies[i].provoked = 1;
        if (result == DMG_DESTROYED) syn_change(-2);
        else if (result == DMG_DISABLED) syn_change(-1);
    }
    if (collide_shooter_team == TEAM_PLAYER && e->role == ROLE_PATROL && result == DMG_DESTROYED) syn_change(3);
    if (collide_shooter_team == TEAM_PLAYER) {
        if (e->role == ROLE_PATROL) {
            faction_change(e->faction, result == DMG_DESTROYED ? -25 : result == DMG_DISABLED ? -10 : -2);
        } else if (e->role == ROLE_PIRATE && result != DMG_NONE) {
            faction_change(patrol_faction, result == DMG_DESTROYED ? 4 : 2);
        }
    }
    if (e->role == ROLE_PIRATE && result == DMG_DESTROYED && collide_shooter_team == TEAM_PLAYER)
        bounty_total += e->alien ? 400 + (int) (ship_defs[e->ship.def_index].price / 400)   /* the UFS pays for Veyr */
                                 : 150 + (int) (ship_defs[e->ship.def_index].price / 200);
    if (e->role == ROLE_PIRATE && slot == gang_leader && (result == DMG_DISABLED || result == DMG_DESTROYED))
        gang_scatter();
    if (result == DMG_DISABLED && e->role == ROLE_PIRATE) {
        disables++;
    } else if (result == DMG_DESTROYED) {
        e->used = E_DEAD;
        if (e->role == ROLE_PIRATE)        { e->timer = PIRATE_RESPAWN;   kills++; }
        else if (e->role == ROLE_PATROL)   { e->timer = PATROL_RESPAWN; }
        else if (e->role == ROLE_DEFENDER) { e->used = E_UNUSED; }
        else                               { e->timer = MERCHANT_RESPAWN; merchant_losses++; }
    }
}

/* ------------------------------------------------------------------------ */
/* Drawing                                                                   */
/* ------------------------------------------------------------------------ */

/* Health bars above a ship: hull (green, red when low) over shields (blue).
 * A disabled ship also shows a DISABLED label. */
static void draw_bars(const Ship *s, int cx, int cy) {
    const ShipDef *d = &s->def;
    int top = cy - ship_looks[s->def_index].radius - 9;
    int x = cx - 8;

    int hull_w = (int) (s->hull * 16 / d->hull_max);
    int shield_total = s->shield[0] + s->shield[1] + s->shield[2] + s->shield[3];
    int shield_w = d->shield_arc_max ? shield_total * 16 / (4 * (int) d->shield_arc_max) : 0;   /* 0 once the shield gens are shot away */

    TILE *bar = (TILE *) gfx_prim(LAYER_BARS, sizeof(TILE));
    if (bar) {
        setTile(bar);
        setXY0(bar, x, top);
        setWH(bar, hull_w, 2);
        if (ship_disabled(s))  setRGB0(bar, 255, 160, 40);
        else if (hull_w > 8)   setRGB0(bar, 60, 220, 60);
        else                   setRGB0(bar, 230, 70, 50);
    }
    if (shield_w > 0) {
        TILE *sh = (TILE *) gfx_prim(LAYER_BARS, sizeof(TILE));
        if (sh) {
            setTile(sh);
            setXY0(sh, x, top + 3);
            setWH(sh, shield_w, 2);   /* two tall: one pixel would flicker when interlaced */
            setRGB0(sh, 90, 170, 255);
        }
    }
    TILE *bg = (TILE *) gfx_prim(LAYER_BARS_BG, sizeof(TILE));
    if (bg) {
        setTile(bg);
        setXY0(bg, x - 1, top - 1);
        setWH(bg, 18, 7);
        setRGB0(bg, 40, 40, 50);
    }
    if (ship_disabled(s))
        gfx_text(cx - 32, top - 12, LAYER_BARS, "DISABLED");
}

void enemies_draw(s32 cam_x, s32 cam_y, int frame) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &enemies[i];
        if (e->used != E_ALIVE) continue;

        int cx = SCREEN_W / 2 + FROM_FP(e->ship.x - cam_x);
        int cy = SCREEN_H / 2 + FROM_FP(e->ship.y - cam_y);
        if (cx < -40 || cx > SCREEN_W + 40 || cy < -40 || cy > SCREEN_H + 40)
            continue;

        ship_draw(&e->ship, cam_x, cam_y, frame);
        draw_bars(&e->ship, cx, cy);
    }
}
