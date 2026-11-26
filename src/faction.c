#include "clock.h"
#include "faction.h"
#include "galaxy.h"

static int rep[FAC_COUNT];

static int syn;
static void choose_sides(void);
int syn_debt, syn_debt_due;
static u8 debt_called;
int syn_hold_level;

void faction_init(void) { rep[0] = rep[1] = 0; syn = 0; syn_hold_level = 0; syn_debt = syn_debt_due = 0; debt_called = 0; }

static const s8 syn_needs[5] = { -100, 10, 30, 55, 80 };
int syn_rep(void) { return syn; }
int syn_level_needs(int level) { return syn_needs[level]; }
int syn_level(void) {
    int l = 0;
    while (l < SYN_INNER_CIRCLE && syn >= syn_needs[l + 1]) l++;
    return l;
}
const char *syn_level_name(int level) {
    static const char *n[5] = { "UNKNOWN", "RECOGNIZED", "TRUSTED", "ESTABLISHED", "INNER CIRCLE" };
    if (syn <= SYN_MARKED && level == syn_level()) return "MARKED";
    return n[level];
}
int syn_friendly(void) { return syn >= syn_needs[SYN_TRUSTED]; }
int syn_hold_units(void) { static const u8 u[4] = { 0, 10, 25, 50 }; return u[syn_hold_level & 3]; }

int faction_rep(int fac) { return fac >= 0 && fac < FAC_COUNT ? rep[fac] : 0; }

const char *faction_name(int fac) { return fac == FAC_FEDERATION ? "UFS" : fac == FAC_REBEL ? "FSA" : "NONE"; }

const char *faction_standing(int fac) {
    int r = faction_rep(fac);
    if (r >= REP_ALLY) return "ALLY";
    if (r >= 25) return "FRIENDLY";
    if (r > REP_HOSTILE) return "NEUTRAL";
    if (r > -60) return "UNFRIENDLY";
    return "HOSTILE";
}

int faction_hostile(int fac) { return fac >= 0 && fac < FAC_COUNT && rep[fac] <= REP_HOSTILE; }

int faction_of_owner(int owner) {
    return owner == OWN_FEDERATION ? FAC_FEDERATION : owner == OWN_REBEL ? FAC_REBEL : FAC_NONE;
}

int faction_patrols(int owner) {
    return owner == OWN_FEDERATION ? 4 : owner == OWN_REBEL ? 3 : 0;
}

static int clamp(int v) { return v < -100 ? -100 : v > 100 ? 100 : v; }

void faction_clear_negative(void) { for (int f = 0; f < FAC_COUNT; f++) if (rep[f] < 0) rep[f] = 0; }

void faction_set(int fac, int v) { if (fac >= 0 && fac < FAC_COUNT) rep[fac] = clamp(v); }
void syn_set(int v) { syn = clamp(v); }


const char *syn_debt_check(void) {
    if (syn_debt <= 0 || debt_called || clock_day() <= syn_debt_due) return 0;
    debt_called = 1;
    if (syn > -40) syn = -40;
    return "YOUR SYNDICATE DEBT IS OVERDUE. THE COLLECTORS ARE COMING";
}

u8 *faction_ser(u8 *p, int load) {
    SER(p, load, rep); SER(p, load, syn); SER(p, load, syn_hold_level);
    SER(p, load, syn_debt); SER(p, load, syn_debt_due); SER(p, load, debt_called);
    return p;
}

void syn_change(int delta) { syn = clamp(syn + delta); choose_sides(); }

/* Choosing a side (M14f). Slightly in favour with both is fine, but once one
 * side fully trusts you (ALLY, 60), its enemy treats you as one of theirs:
 *   UFS ally  -> the FSA is hostile, and the Syndicates won't deal with you;
 *   FSA ally  -> the UFS is hostile;
 *   Syndicate ESTABLISHED -> the UFS is hostile.
 * Losing that trust lifts the cap again, so changing sides is possible, but slow. */
static const char *side_msg;
const char *faction_take_message(void) { const char *m = side_msg; side_msg = 0; return m; }

static void cap(int *v, int max, const char *msg) {
    if (*v > max) { if (*v > REP_HOSTILE) side_msg = msg; *v = max; }
}

static void choose_sides(void) {
    if (rep[FAC_FEDERATION] >= REP_ALLY) {
        cap(&rep[FAC_REBEL], -40, "THE FSA NOW COUNTS YOU AS A UFS LOYALIST: THEY ARE HOSTILE");
        cap(&syn, SYN_MARKED, "THE SYNDICATES WON'T DEAL WITH A UFS LOYALIST");
    }
    if (rep[FAC_REBEL] >= REP_ALLY)
        cap(&rep[FAC_FEDERATION], -40, "THE UFS NOW COUNTS YOU AS A REBEL: THEY ARE HOSTILE");
    if (syn >= syn_needs[SYN_ESTABLISHED])
        cap(&rep[FAC_FEDERATION], -40, "THE UFS HAS YOU ON FILE AS A SYNDICATE MAN: THEY ARE HOSTILE");
}

void faction_change(int fac, int delta) {
    if (fac < 0 || fac >= FAC_COUNT) return;
    rep[fac] = clamp(rep[fac] + delta);
    int rival = 1 - fac;
    rep[rival] = clamp(rep[rival] - delta / 2);
    choose_sides();
}
