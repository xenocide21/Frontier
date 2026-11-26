/*
 * FRONTIER - the lounge and escort hiring (Milestone 14b). See lounge.h.
 */
#include <stdio.h>

#include "clock.h"
#include "enemy.h"
#include "faction.h"
#include "galaxy.h"
#include "lounge.h"
#include "veyr.h"
#include "save.h"
#include "market.h"
#include "mission.h"
#include "outfit.h"
#include "rng.h"
#include "save.h"
#include "ship.h"
#include "title.h"
#include "world.h"

static char rumours[LOUNGE_RUMOURS][120];
static int  n_rumours;
static char encounter[120], offer_line[80], result[64];
static Mission offer;
static int  has_offer, offer_taken;
static u32  offer_key, taken_key;

/* The lounge changes four times a day. */
static u32 seed_for(int sys, int nav, int salt) {
    u32 k = (u32) (clock_day() * 977 + (clock_hour() / 6) * 131 + sys * 17 + nav * 5 + salt);
    return k * 2654435761u ^ 0x9e3779b9u;
}

static int other_sys(int sys) {
    int s = galaxy_random_near(sys, 4, 1);      /* talk is about places within reach (M16a) */
    return s >= 0 ? s : sys;
}

/* A commodity this place imports (true) or exports (!want_import), at system s. */
static int pick_commodity(int s, int want_import) {
    for (int tries = 0; tries < 30; tries++) {
        int c = rng_range(COMM_COUNT);
        if (commodities[c].contraband) continue;
        int r = market_role_at(s, rng_range(2), c);
        if (want_import ? r < 0 : r > 0) return c;
    }
    return rng_range(COMM_NARCOTICS);
}

static void make_rumour(char *out, int len, int sys, int kind) {
    static const char *admiral[PILOT_COUNT] = { "GOD HELP US ALL", "CHECK THE CATERING BUDGET",
                                                "CLEAR THE LANES", "PLEASE LET IT BE A COMET" };
    static const char *surnames[PILOT_COUNT] = { "BEEFSTEEL", "FANSHAWE-BUTTERSWORTH", "BLASTWELL", "QUASAR-PENNYWHISTLE" };
    switch (kind) {
    case 0: case 1: {                                    /* a trade tip: 3 in 4 are good */
        int s = other_sys(sys);
        int good = rng_range(4) != 0;
        int imp = rng_range(2);
        int c = good ? pick_commodity(s, imp) : rng_range(COMM_NARCOTICS);
        if (imp) snprintf(out, len, "A HAULER SAYS %s IS PAYING TOP CREDITS FOR|%s. HE SOUNDS SURE OF IT.",
                          galaxy_sys(s)->name, commodities[c].name);
        else     snprintf(out, len, "\"%s IS DIRT CHEAP AT %s,\" SAYS A TRADER.|\"FILL YOUR HOLD WHILE IT LASTS.\"",
                          commodities[c].name, galaxy_sys(s)->name);
        break;
    }
    case 2: {                                            /* danger */
        int s = galaxy_random_near(sys, 4, 0); if (s < 0) s = sys;
        const StarSystem *g = galaxy_sys(s);
        if (galaxy_empty(s))
            snprintf(out, len, "\"NOTHING AT %s BUT DARK AND AMBUSHES. NO PORT,|NO FUEL. GO IN WITH A FULL TANK OR A SCOOP.\"", g->name);
        else if (g->band >= BAND_FRONTIER)
            snprintf(out, len, "\"SYNDICATE GANGS ARE THICK OUT AT %s. I SAW A|%s LEADING SIX OF THEM.\"", g->name,
                     g->band == BAND_RIM ? "MARAUDER" : "JACKAL");
        else
            snprintf(out, len, "\"%s IS QUIET. THE UFS SCANS EVERYONE THOUGH,|SO DON'T CARRY ANYTHING YOU CAN'T EXPLAIN.\"", g->name);
        break;
    }
    case 3: {                                            /* talk about you */
        int pid = pilot_id < PILOT_COUNT ? pilot_id : 0;
        if (enemies_player_pirate_kills() >= 10)
            snprintf(out, len, "SOMEONE POINTS AT YOU: \"THAT'S %s. THE ONE|WHO KEEPS SHOOTING PIRATES.\" THE ROOM GOES QUIET.", pilot_name);
        else if (syn_friendly())
            snprintf(out, len, "A MAN IN A RUSTED JACKET NODS AT YOU. \"THE|SYNDICATE SPEAKS WELL OF YOU, %s.\"", surnames[pid]);
        else if (faction_rep(FAC_FEDERATION) <= REP_HOSTILE)
            snprintf(out, len, "\"THERE WERE UFS PEOPLE IN HERE ASKING ABOUT|%s,\" THE BARTENDER MUTTERS.", surnames[pid]);
        else
            snprintf(out, len, "ON THE NEWSFEED AN ADMIRAL ENDS A GRAVE SPEECH:|\"AND IF %s IS INVOLVED, %s.\"", surnames[pid], admiral[pid]);
        break;
    }
    default: {                                           /* something out in the dark */
        int s = galaxy_random_near(sys, 5, 0); if (s < 0) s = sys;
        if (veyr_phase() == VP_INVADING && rng_range(2)) {       /* the war, in the bar (M17a) */
            if (veyr_revealed())
                snprintf(out, len, "\"THEY SAY THE HIVE IS PAST %s. NOBODY WHO|WENT IN HAS COME BACK.\"", galaxy_sys(VEYR_DOOR_A)->name);
            else
                snprintf(out, len, "\"THE VEYR COME FROM SOMEWHERE. KILL ENOUGH OF|THEM AND MAYBE THEIR NAV CORES TELL YOU WHERE.\"");
        } else if (veyr_phase() == VP_DORMANT && play_frames > VEYR_DEADLINE * 4 / 5 && rng_range(2))
            snprintf(out, len, "\"THE RIM HAS GONE QUIET,\" A SYNDICATE PILOT|MUTTERS. \"TOO QUIET. SOMETHING'S COMING.\"");
        else if (!missions_discovered(s))
            snprintf(out, len, "AN OLD MINER SWEARS SOMETHING DRIFTS OUT IN|THE DEEP OF %s. \"FOLLOW THE COLD,\" HE SAYS.", galaxy_sys(s)->name);
        else
            snprintf(out, len, "\"THE VEYR ARE OUT THERE, PAST THE RIM,\" SAYS A|DRUNK PILOT. NOBODY LAUGHS.");
        break;
    }
    }
}

void lounge_make(int sys, int nav) {
    u32 saved = rng_state;
    rng_state = seed_for(sys, nav, 0);

    n_rumours = 1 + rng_range(LOUNGE_RUMOURS);          /* not every visit is fruitful */
    int used = 0;
    for (int i = 0; i < n_rumours; i++) {
        int kind;
        do kind = rng_range(5); while (used & (1 << kind));
        used |= 1 << kind;
        make_rumour(rumours[i], sizeof rumours[i], sys, kind);
    }

    offer_key = seed_for(sys, nav, 1);
    has_offer = 0;
    switch (rng_range(4)) {
    case 0:
        snprintf(encounter, sizeof encounter, "NOBODY WORTH TALKING TO TONIGHT.");
        break;
    case 1:
        missions_make_custom(&offer, MI_PASSENGER, sys, nav, 160);
        offer.units = (u8) (1 + rng_range(3));
        snprintf(encounter, sizeof encounter, "A NERVOUS MAN IN A GOOD SUIT NEEDS PASSAGE TO|%s. QUIETLY. HE'LL PAY DOUBLE.",
                 offer.dest_nav == NAV_STATION ? galaxy_sys(offer.dest_sys)->station_name : galaxy_sys(offer.dest_sys)->planet_name);
        has_offer = 1;
        break;
    case 2:
        missions_make_custom(&offer, MI_BOUNTY, sys, nav, 130);
        snprintf(encounter, sizeof encounter, "A RETIRED BOUNTY HUNTER LEANS IN: \"%s IS|HIDING IN %s. I'M TOO OLD. YOU GO.\"",
                 missions_wanted_name(offer.name), galaxy_sys(offer.dest_sys)->name);
        has_offer = 1;
        break;
    default:
        missions_make_custom(&offer, MI_SURVEY, sys, nav, 120);
        if (missions_discovered(offer.dest_sys)) {
            snprintf(encounter, sizeof encounter, "A SCIENTIST IS LOOKING FOR A PILOT, BUT SHE|LEAVES BEFORE YOU FINISH YOUR DRINK.");
        } else {
            snprintf(encounter, sizeof encounter, "A SCIENTIST WANTS SOMEONE TO SURVEY A SIGNAL|IN %s. SHE PAYS WELL FOR GOOD DATA.",
                     galaxy_sys(offer.dest_sys)->name);
            has_offer = 1;
        }
        break;
    }
    if (offer_key == taken_key) has_offer = 0;
    if (has_offer) snprintf(offer_line, sizeof offer_line, "TAKE THE JOB: PAYS %d", (int) offer.reward);
    rng_state = saved;
}

const char *lounge_rumour(int i) { return i < n_rumours ? rumours[i] : 0; }
const char *lounge_encounter(void) { return encounter; }
int lounge_has_offer(void) { return has_offer; }
const char *lounge_offer_line(void) { return offer_line; }

const char *lounge_take_offer(int cargo_cap) {
    if (!has_offer) return 0;
    const char *r = missions_accept_custom(&offer, cargo_cap);
    if (r && r[0] == 'D') { has_offer = 0; taken_key = offer_key; }
    return r;
}

/* Gambling: fixed odds, shown up front. The house edge is about 8-11%. */
const s32 lounge_bets[4] = { 100, 500, 1000, 5000 };
static const char *game_names[GAME_COUNT] = { "HIGH CARD", "LUCKY SEVENS" };
static const u8 game_win_pct[GAME_COUNT] = { 46, 22 };    /* tie loses / 7 or 11 on two dice (8 in 36) */
static const u8 game_pays[GAME_COUNT] = { 2, 4 };

const char *lounge_game_line(int game, int bet) {
    snprintf(result, sizeof result, "%s  BET < %d >  WINS %d%%  PAYS %dX", game_names[game], (int) lounge_bets[bet],
             game_win_pct[game], game_pays[game]);
    return result;
}

const char *lounge_gamble(int game, int bet_index) {
    static const char *cards[13] = { "2", "3", "4", "5", "6", "7", "8", "9", "10", "JACK", "QUEEN", "KING", "ACE" };
    s32 bet = lounge_bets[bet_index];
    if (credits < bet) return "YOU CAN'T COVER THAT BET";
    credits -= bet;
    int win;
    if (game == GAME_HIGH_CARD) {
        int you = rng_range(13), dealer = rng_range(13);
        win = you > dealer;
        snprintf(result, sizeof result, "YOU DRAW A %s, THE DEALER A %s. %s", cards[you], cards[dealer],
                 win ? "YOU WIN!" : you == dealer ? "TIE: HOUSE WINS." : "YOU LOSE.");
    } else {
        int a = 1 + rng_range(6), b = 1 + rng_range(6);
        win = a + b == 7 || a + b == 11;
        snprintf(result, sizeof result, "THE DICE SHOW %d AND %d: %d. %s", a, b, a + b, win ? "YOU WIN!" : "YOU LOSE.");
    }
    if (win) credits += bet * game_pays[game];
    return result;
}

/* ------------------------------------------------------------------------ */
/* Escort hiring                                                             */
/* ------------------------------------------------------------------------ */

typedef struct { u8 def, days, name; s32 fee; } Hire;
static Hire hires[HIRE_OFFERS];
static int n_hires;
static u32 hire_key, hired_keys[8];
static int n_hired_keys;
static char hire_buf[80];

static const char *hire_names[8] = { "ACE MORROW", "TWO-GUNS TALLY", "KIT VARGA", "OLD JONAS",
                                     "BRIGHT NELL", "SOFT-SPOKEN SAM", "DUCHESS PIKE", "LUCKY OKONKWO" };

void hire_make(int sys, int nav) {
    static const u8 ships[6] = { DEF_RAIDER, DEF_RAIDER, DEF_WASP, DEF_HAMMER, DEF_VANGUARD, DEF_RENEGADE };
    hire_key = seed_for(sys, nav, 7) & ~0xFu;
    u32 saved = rng_state;
    rng_state = hire_key;
    n_hires = 0;
    for (int i = 0; i < HIRE_OFFERS; i++) {
        int gone = 0;
        Hire h;
        h.def = ships[rng_range(6)];
        h.days = (u8) (3 + rng_range(3) * 2);
        h.name = (u8) rng_range(8);
        h.fee = (s32) (ship_defs[h.def].price / 60 + 150) * h.days;
        for (int k = 0; k < n_hired_keys; k++) if (hired_keys[k] == (hire_key | (u32) i)) gone = 1;
        if (gone) continue;
        hires[n_hires] = h;
        hires[n_hires].name |= (u8) (i << 4);          /* remember its slot */
        n_hires++;
    }
    rng_state = saved;
}

int hire_count(void) { return n_hires; }

const char *hire_line(int i) {
    const Hire *h = &hires[i];
    snprintf(hire_buf, sizeof hire_buf, "%s IN A %s  %d DAYS  %d", hire_names[h->name & 7],
             ship_looks[h->def].name, h->days, (int) h->fee);
    return hire_buf;
}

const char *hire_take(int i) {
    if (i < 0 || i >= n_hires) return 0;
    const Hire *h = &hires[i];
    if (fleet_count >= FLEET_MAX) return "YOUR HANGAR IS FULL";
    if (credits < h->fee) return "NOT ENOUGH CREDITS";
    credits -= h->fee;
    fleet_add(h->def);
    fleet_hired[fleet_count - 1] = (u16) (clock_day() + h->days);
    if (n_hired_keys < 8) hired_keys[n_hired_keys++] = hire_key | (u32) (h->name >> 4);
    else { for (int k = 0; k < 7; k++) hired_keys[k] = hired_keys[k + 1]; hired_keys[7] = hire_key | (u32) (h->name >> 4); }
    snprintf(hire_buf, sizeof hire_buf, "%s SIGNS ON FOR %d DAYS. THEY FLY WITH YOU", hire_names[h->name & 7], h->days);
    for (int k = i; k < n_hires - 1; k++) hires[k] = hires[k + 1];
    n_hires--;
    return hire_buf;
}
