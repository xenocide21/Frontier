#include <stdio.h>
#include <string.h>

#include "clock.h"
#include "conquest.h"
#include "enemy.h"
#include "faction.h"
#include "galaxy.h"
#include "outfit.h"
#include "save.h"
#include "weapon.h"
#include "world.h"
#include "veyr.h"
#include "mission.h"


/* Each owner's defence: light ships, heavy ships and the flagship that leads
 * the last wave. Placeholders, to be balanced with the full galaxy. */
typedef struct { u8 light[2], heavy[2], flag; } Defence;
static const Defence veyr_defence = { { DEF_SKITTER, DEF_MANTIS }, { DEF_REAVER, DEF_BROODGUARD }, DEF_BROODGUARD };
static const Defence hive_defence = { { DEF_MANTIS, DEF_REAVER }, { DEF_BROODGUARD, DEF_REAVER }, DEF_LEVIATHAN };
static const Defence defences[OWN_PLAYER] = {
    { { DEF_TALON, DEF_VANGUARD },           { DEF_SENTINEL, DEF_VANGUARD }, DEF_RESOLUTE },  /* UFS */
    { { DEF_WAYFINDER, DEF_FRONTIER_RUNNER }, { DEF_WATCHMAN, DEF_RECLAIMER }, DEF_WATCHMAN }, /* independent militia */
    { { DEF_WASP, DEF_RENEGADE },            { DEF_HAMMER, DEF_DEFIANT },    DEF_REPRISAL },  /* FSA */
    { { DEF_SCRAPPER, DEF_JACKAL },          { DEF_CHOPSHOP, DEF_JACKAL },   DEF_MARAUDER },  /* Syndicate */
};

static int active, sys, wave, waves, timer, owner;
static int last_day;                   /* tribute is paid when the day changes */
static u8  war_won;                    /* the post-war campaign is done (M18b) */
static char msg[64];

void conquest_init(void) { active = 0; last_day = 0; war_won = 0; }
int  conquest_active(void) { return active; }

static const NavObject *base(void) { return world_nav(galaxy_sys(galaxy_current())->base); }

const char *conquest_target_name(void) {
    const StarSystem *s = galaxy_sys(galaxy_current());
    if (galaxy_empty(galaxy_current())) return "NOTHING";
    return s->base == NAV_STATION ? s->station_name : s->planet_name;
}

static const char *who(int own) {
    return own == OWN_FEDERATION ? "THE UFS" : own == OWN_REBEL ? "THE FSA" :
           own == OWN_SYNDICATE ? "THE SYNDICATES" : own == OWN_PLAYER ? "YOU" : own == OWN_NONE ? "NOBODY" :
           own == OWN_VEYR ? "THE VEYR" : "THE INDEPENDENTS";
}
const char *conquest_owner_name(void) { return who(galaxy_sys(galaxy_current())->owner); }

static int base_dist(const Ship *pl) {
    const NavObject *b = base();
    return (int) approx_len((int) FROM_FP(pl->x) - b->x, (int) FROM_FP(pl->y) - b->y);
}

const char *conquest_why_not(const Ship *pl) {
    if (active) return "THE ASSAULT IS UNDER WAY";
    if (galaxy_sys(galaxy_current())->owner == OWN_PLAYER) return "THIS SYSTEM IS ALREADY YOURS";
    if (galaxy_empty(galaxy_current())) return "EMPTY SPACE: NOTHING HERE TO TAKE";
    if (!ship_alive(pl) || ship_disabled(pl)) return "NOT IN THIS STATE";
    if (base_dist(pl) > ASSAULT_RANGE) {
        snprintf(msg, sizeof msg, "FLY CLOSER TO %s TO ATTACK IT", conquest_target_name());
        return msg;
    }
    return 0;
}

int conquest_tribute_of(int s) { return (300 + 150 * (galaxy_sys(s)->security & 3)) * veyr_settled_pct(s) / 100; }

/* The post-war campaign (M18b): who your side wants beaten. */
static int war_enemy(int own) {
    int side = veyr_hero_side();
    if (side == OWN_FEDERATION) return own == OWN_REBEL;
    if (side == OWN_REBEL)      return own == OWN_FEDERATION;
    if (side == OWN_SYNDICATE)  return own == OWN_FEDERATION || own == OWN_REBEL;
    return 0;
}
int conquest_war_left(void) {
    if (veyr_hero_side() < 0) return -1;
    int n = 0;
    for (int s = 0; s < GALAXY_SYSTEMS; s++)
        if (!galaxy_empty(s) && war_enemy(galaxy_sys(s)->owner)) n++;
    return n;
}
int conquest_war_won(void) { return war_won; }
const char *conquest_war_status(void) {
    static char b[56];
    int side = veyr_hero_side(), n = conquest_war_left();
    if (side < 0) return 0;
    if (war_won || n == 0)
        return side == OWN_FEDERATION ? "VICTORY: THE FSA HAS KNELT TO THE UFS" :
               side == OWN_REBEL ? "VICTORY: EVERY WORLD IS FREE OF THE UFS" : "VICTORY: THE GALAXY WEARS YOUR CROWN";
    snprintf(b, sizeof b, side == OWN_FEDERATION ? "THE WAR: %d FSA WORLD%s TO BRING TO HEEL" :
             side == OWN_REBEL ? "THE WAR: %d UFS WORLD%s TO FREE" : "THE WAR: %d NAVY WORLD%s TO TAKE", n, n == 1 ? "" : "S");
    return b;
}

/* The whole faction turns on you; independents only fight for their colony. */
void conquest_declare(int s) {
    int own = galaxy_sys(s)->owner;
    if (own == OWN_FEDERATION && faction_rep(FAC_FEDERATION) > -60) faction_set(FAC_FEDERATION, -60);
    if (own == OWN_REBEL && faction_rep(FAC_REBEL) > -60) faction_set(FAC_REBEL, -60);
    if (own == OWN_SYNDICATE && syn_rep() > SYN_MARKED - 15) syn_set(SYN_MARKED - 15);
}

static int waves_of(int s) {
    if (s == VEYR_PRIME) return 5 + game_difficulty % 3;                 /* the hive: 5 to 7 waves (M17a) */
    if (galaxy_sys(s)->owner == OWN_VEYR) return 3 + game_difficulty % 3;
    return 2 + (galaxy_sys(s)->security & 3);                            /* 2 out on the Rim .. 5 in the Core */
}

/* The ships of wave w (0-based) of n, for an owner: returns how many. */
static int wave_sys;                       /* the system the waves are for (the hive has its own) */
static int wave_defs(int own, int w, int n, u8 *defs) {
    const Defence *d = own == OWN_VEYR ? (wave_sys == VEYR_PRIME ? &hive_defence : &veyr_defence)
                                       : &defences[own < OWN_PLAYER ? own : OWN_INDEPENDENT];
    int last = w == n - 1;
    int size = 2 + w;                                     /* each wave a ship bigger */
    if (size > 5) size = 5;
    int heavy = w / 2;
    int k = 0;
    if (last) defs[k++] = d->flag;
    for (int i = 0; i < heavy && k < 6; i++) defs[k++] = d->heavy[i & 1];
    while (k < size + last && k < 6) { defs[k] = d->light[k & 1]; k++; }
    return k;
}

/* Every defender a system would launch, as one number (for squadron attacks, M15b). */
int conquest_defence_strength(int s) {
    int own = galaxy_sys(s)->owner, n = waves_of(s), total = 0;
    wave_sys = s;
    u8 defs[6];
    for (int w = 0; w < n; w++) {
        int k = wave_defs(own, w, n, defs);
        for (int i = 0; i < k; i++) total += def_strength(defs[i]);
    }
    return total;
}

const char *conquest_start(const Ship *pl) {
    (void) pl;
    sys = galaxy_current();
    owner = galaxy_sys(sys)->owner;
    waves = waves_of(sys);
    wave = 0;
    timer = 120;
    active = 1;
    conquest_declare(sys);
    snprintf(msg, sizeof msg, "%s SCRAMBLES ITS DEFENCE: %d WAVES", conquest_target_name(), waves);
    return msg;
}

static void launch(const Ship *pl) {
    u8 defs[6];
    int last = wave == waves - 1;
    wave_sys = sys;
    int k = wave_defs(owner, wave, waves, defs);
    int team = owner == OWN_FEDERATION ? TEAM_FED : owner == OWN_REBEL ? TEAM_REBEL : TEAM_ENEMY;
    const NavObject *b = base();
    enemies_siege_wave(defs, k, TO_FP(b->x), TO_FP(b->y), team, pl->x, pl->y);
    wave++;
    if (last) snprintf(msg, sizeof msg, "FINAL WAVE: A %s LEADS %d SHIPS OUT", ship_looks[defs[0]].name, k);
    else      snprintf(msg, sizeof msg, "WAVE %d OF %d: %d SHIPS LAUNCHING", wave, waves, k);
}

void conquest_abort_quiet(void) {
    if (active) enemies_siege_end();
    active = 0;
}

const char *conquest_capture(int s) {
    int prev = galaxy_sys(s)->owner;
    int side = veyr_hero_side();
    if (war_enemy(prev) && side != OWN_SYNDICATE) {          /* M18b: taken for your navy, under its flag */
        galaxy_set_owner(s, side);
        if (s == galaxy_current()) {
            const StarSystem *ss = galaxy_sys(s);
            enemies_set_system(ss->pirates, ss->merchants, faction_patrols(side), faction_of_owner(side));
            enemies_siege_end();
        }
        active = 0;
        int prize = 4000 + 2000 * (galaxy_sys(s)->security & 3);
        credits += prize;
        int left = conquest_war_left();
        if (left == 0 && !war_won) {
            war_won = 1;
            credits += 100000;
            if (side == OWN_FEDERATION)
                veyr_banner("THE FSA KNEELS", "THE LAST FREE WORLD RAISES THE UFS FLAG. THE WAR IS OVER.",
                            "THE UFS THANKS ITS ADMIRAL: 100000 CR.");
            else
                veyr_banner("THE FREE STATES ARE FREE", "THE LAST UFS GARRISON SURRENDERS. THE WAR IS OVER.",
                            "THE FSA THANKS ITS MARSHAL: 100000 CR.");
            snprintf(msg, sizeof msg, "%s TAKEN. VICTORY! +%d CR", galaxy_sys(s)->name, prize + 100000);
        } else
            snprintf(msg, sizeof msg, "%s FLIES THE %s FLAG. PRIZE %d CR. %d LEFT", galaxy_sys(s)->name,
                     side == OWN_FEDERATION ? "UFS" : "FSA", prize, left);
        return msg;
    }
    galaxy_set_owner(s, OWN_PLAYER);
    if (s == galaxy_current()) enemies_set_veyr(veyr_swarm_chance(s));
    if (s == galaxy_current()) {
        const StarSystem *ss = galaxy_sys(s);
        enemies_set_system(ss->pirates, ss->merchants, 0, FAC_NONE);   /* no patrols of theirs any more */
        enemies_siege_end();
    }
    active = 0;
    snprintf(msg, sizeof msg, "%s IS YOURS. TRIBUTE: %d CR A DAY", galaxy_sys(s)->name, conquest_tribute_of(s));
    if (war_enemy(prev)) {                                    /* M18b: the crown's war */
        int left = conquest_war_left();
        if (left == 0 && !war_won) {
            war_won = 1;
            credits += 100000;
            veyr_banner("THE RIM CROWN", "THE LAST NAVY WORLD FALLS. THE GALAXY IS YOURS.",
                        "THE SYNDICATES PAY HOMAGE: 100000 CR.");
            snprintf(msg, sizeof msg, "%s IS YOURS. THE GALAXY IS YOURS", galaxy_sys(s)->name);
        } else
            snprintf(msg, sizeof msg, "%s IS YOURS. %d NAVY WORLD%s LEFT", galaxy_sys(s)->name, left, left == 1 ? "" : "S");
    }
    const char *v = veyr_on_capture(s, prev);       /* a Veyr world: maybe the hive's position, or the hive itself */
    return v ? v : msg;
}

const char *conquest_update(const Ship *pl) {
    if (!active) return 0;
    if (galaxy_current() != sys || !ship_alive(pl) || (wave > 0 && !enemies_siege())) {
        conquest_abort_quiet();
        snprintf(msg, sizeof msg, "ASSAULT CALLED OFF. %s WILL NOT FORGET IT", who(owner));
        return msg;
    }
    if (wave > 0 && enemies_defenders_left() > 0) return 0;
    if (wave >= waves) return conquest_capture(sys);
    if (timer == 0 && wave > 0) {
        timer = 300;                                      /* a breather between waves */
        snprintf(msg, sizeof msg, "WAVE %d BEATEN. THE NEXT IS LAUNCHING", wave);
        return msg;
    }
    if (timer > 0 && --timer > 0) return 0;
    launch(pl);
    return msg;
}

int conquest_hud(char *buf, int n) {
    if (!active) return 0;
    if (wave == 0) snprintf(buf, n, "ASSAULT ON %s: DEFENCE SCRAMBLING", galaxy_sys(sys)->name);
    else if (enemies_defenders_left() == 0) snprintf(buf, n, "ASSAULT: WAVE %d OF %d NEXT", wave + 1, waves);
    else snprintf(buf, n, "ASSAULT: WAVE %d OF %d  (%d LEFT)", wave, waves, enemies_defenders_left());
    return 1;
}

const char *conquest_tribute(void) {
    int day = clock_day();
    if (last_day == 0) { last_day = day; return 0; }
    if (day == last_day) return 0;
    int days = day - last_day;
    last_day = day;
    int total = 0, n = 0;
    for (int s = 0; s < GALAXY_SYSTEMS; s++)
        if (galaxy_sys(s)->owner == OWN_PLAYER) { total += conquest_tribute_of(s) * days; n++; }
    int side = veyr_hero_side();
    int pay = side >= 0 ? 1500 * days : 0;          /* the admiral's, the marshal's, the crown's (M17b, M18b) */
    if (!total && !pay) return 0;
    credits += total + pay;
    const char *what = side == OWN_FEDERATION ? "ADMIRAL'S PAY" : side == OWN_REBEL ? "MARSHAL'S PAY" : "THE CROWN'S CUT";
    if (!total) snprintf(msg, sizeof msg, "%s: %d CR", what, pay);
    else if (pay) snprintf(msg, sizeof msg, "TRIBUTE %d CR, %s %d CR", total, what, pay);
    else snprintf(msg, sizeof msg, "TRIBUTE FROM YOUR %d SYSTEM%s: %d CR", n, n == 1 ? "" : "S", total);
    return msg;
}

u8 *conquest_ser(u8 *p, int load) {
    if (!load || ser_version >= 5) SER(p, load, last_day);
    if (!load || ser_version >= 12) SER(p, load, war_won); else war_won = 0;
    if (load) active = 0;
    return p;
}
