#include "faction.h"
#include "clock.h"
#include <stdio.h>
#include <string.h>

#include "outfit.h"
#include "rng.h"
#include "conquest.h"
#include "market.h"
#include "galaxy.h"
#include "save.h"

int  credits;
char pilot_name[PILOT_NAME_MAX];
u8   mod_level[MOD_COUNT];
int  fuel, fuel_max;

static const char *names[MOD_COUNT] = { "MASS EXPANSION", "STORAGE EXPANSION", "MANEUVERING", "RADAR UPGRADE",
                                        "ANTIMATTER SCOOP", "RADAR JAMMER", "FALSE TRANSPONDER",
                                        "VEYR BIO-REGEN HULL", "VEYR NERVE LINK" };
static const char *effects[MOD_COUNT] = {
    "+20% MASS ALLOWANCE PER LEVEL: CARRY HEAVIER MODULES",
    "+20% CARGO CAPACITY PER LEVEL",
    "+8% TURN RATE PER LEVEL (THRUSTERS AND GYROS)",
    "+600 PX RADAR RANGE (BASE 1200)",
    "GATHERS FUEL IN DEAD SPACE, FASTER EACH LEVEL",
    "PATROLS AND PIRATES SPOT YOU 140 PX LATER PER LEVEL",
    "HALF THE TIME, UFS PATROLS WAVE YOU ON WITHOUT A SCAN",
    "LIVING CHITIN: THE HULL KNITS ITSELF IN FLIGHT",
    "VEYR NERVES WIRED TO THE REACTOR: +25% ENERGY RECHARGE"
};
static const u32 base_price[MOD_COUNT] = { 2000, 1500, 2500, 0, 3000, 0, 0, 40000, 30000 };
static const u16 radar_prices[3] = { 1000, 2500, 4500 };
static const u16 jammer_prices[3] = { 8000, 16000, 30000 };

int mod_max(int mod) {
    return mod == MOD_RADAR || mod == MOD_JAMMER || mod_alien(mod) ? 3 : mod == MOD_TRANSPONDER ? 1 : mod == MOD_SCOOP ? 5 : MOD_MAX_LEVEL;
}
int mod_alien(int mod) { return mod == MOD_BIOREGEN || mod == MOD_NERVELINK; }
int mod_illegal(int mod) { return mod == MOD_JAMMER || mod == MOD_TRANSPONDER; }
int mod_needs_syn(int mod) { return mod == MOD_JAMMER ? SYN_RECOGNIZED : mod == MOD_TRANSPONDER ? SYN_TRUSTED : 0; }
int outfit_mass_pct(void) { return 100 + MASS_PCT * mod_level[MOD_MASS]; }

/* Price of reaching level lv (1 .. max): base x 2.2^(lv - 1). */
static int level_price(int mod, int lv) {
    if (mod == MOD_RADAR) return radar_prices[lv - 1];
    if (mod == MOD_JAMMER) return jammer_prices[lv - 1];
    if (mod == MOD_TRANSPONDER) return 25000;
    u32 p = base_price[mod];
    for (int i = 1; i < lv; i++) p = p * 22 / 10;
    return (int) p;
}

void outfit_default_name(void) {
    strncpy(pilot_name, "CHAD BEEFSTEEL", PILOT_NAME_MAX - 1);
    pilot_name[PILOT_NAME_MAX - 1] = 0;
}

u8  fleet[FLEET_MAX];
u8  fleet_stance[FLEET_MAX];
int fleet_count;

const char *stance_name(int stance) {
    static const char *n[STANCE_COUNT] = { "HANGAR", "FOLLOW", "GUARD", "ATTACK" };
    return n[stance];
}

u16 fleet_hired[FLEET_MAX];
u8  fleet_sys[FLEET_MAX], fleet_sq[FLEET_MAX], fleet_fuel[FLEET_MAX];
u8  sq_order[SQUADRONS], sq_dest[SQUADRONS], sq_next[SQUADRONS];
u16 sq_eta[SQUADRONS];
u8  sq_mission[SQUADRONS], sq_home[SQUADRONS];
char fleet_pname[FLEET_MAX][FLEET_PILOT_LEN];
u16  fleet_xp[FLEET_MAX];
static int scoop_acc;

static u8 module_store[MT_COUNT][TIER_MAX];
int store_count(int type, int tier) { return module_store[type][(tier ? tier : 1) - 1]; }
void store_add(int type, int tier) { u8 *c = &module_store[type][(tier ? tier : 1) - 1]; if (*c < 255) (*c)++; }
int store_take(int type, int tier) {
    u8 *c = &module_store[type][(tier ? tier : 1) - 1];
    if (*c == 0) return 0;
    (*c)--;
    return 1;
}
int store_rows(void) {
    int n = 0;
    for (int t = 0; t < MT_COUNT; t++) for (int k = 0; k < TIER_MAX; k++) n += module_store[t][k] > 0;
    return n;
}
void store_row(int i, int *type, int *tier) {
    for (int t = 0; t < MT_COUNT; t++)
        for (int k = 0; k < TIER_MAX; k++)
            if (module_store[t][k] > 0 && i-- == 0) { *type = t; *tier = k + 1; return; }
    *type = 0; *tier = 1;
}
int fit_fee(int type, int tier) {
    int f = fit_tier_price(type, tier) * FIT_FEE_PCT / 100;
    return f < FIT_FEE_MIN ? FIT_FEE_MIN : f;
}
int store_sell(int type, int tier) {
    if (!store_take(type, tier)) return 0;
    int v = fit_tier_price(type, tier) * STORE_SELL_PCT / 100;
    credits += v;
    return v;
}
void store_backup(int restore) {
    static u8 copy[MT_COUNT][TIER_MAX];
    if (restore) memcpy(module_store, copy, sizeof copy);
    else         memcpy(copy, module_store, sizeof copy);
}

void fleet_add(int def_index) {
    if (fleet_count >= FLEET_MAX) return;
    fleet_hired[fleet_count] = 0;
    fleet[fleet_count] = (u8) def_index;
    fleet_sys[fleet_count] = (u8) galaxy_current();
    fleet_sq[fleet_count] = 0;
    fleet_fuel[fleet_count] = (u8) ship_defs[def_index].fuel_cap;
    {                                                 /* a pilot comes with the ship */
        static const char *calls[] = { "VIPER", "HAWK", "JINX", "RAZOR", "MOTH", "BISHOP", "DUKE", "NOVA",
            "ROOK", "SPARROW", "BANDIT", "TANK", "PIKE", "MAGPIE", "SAGE", "DIESEL", "KESTREL", "WREN",
            "BOLT", "HEX", "LUCKY", "SMOKE", "ORCA", "GRIT", "DUSTY", "FLINT", "COMET", "BRAVO",
            "IVY", "TORCH", "MERLIN", "BUCK" };
        int n = (int) (sizeof calls / sizeof calls[0]), k = rng_range(n);
        for (int t = 0; t < n; t++, k = (k + 1) % n) {      /* a call sign nobody else in the fleet has */
            int used = 0;
            for (int j = 0; j < fleet_count; j++) used |= strcmp(fleet_pname[j], calls[k]) == 0;
            if (!used) break;
        }
        strncpy(fleet_pname[fleet_count], calls[k], FLEET_PILOT_LEN - 1);
        fleet_pname[fleet_count][FLEET_PILOT_LEN - 1] = 0;
        fleet_xp[fleet_count] = 0;
    }
    fit_default(def_index, &fleet_layout[fleet_count]);
    fleet_stance[fleet_count] = STANCE_FOLLOW;
    fleet_count++;
}

void fleet_remove(int i) {
    if (i < 0 || i >= fleet_count) return;
    for (int k = i; k < fleet_count - 1; k++) {
        fleet[k] = fleet[k + 1];
        fleet_stance[k] = fleet_stance[k + 1];
        fleet_layout[k] = fleet_layout[k + 1];
        fleet_hired[k] = fleet_hired[k + 1];
        fleet_sys[k] = fleet_sys[k + 1];
        fleet_sq[k] = fleet_sq[k + 1];
        fleet_fuel[k] = fleet_fuel[k + 1];
        memcpy(fleet_pname[k], fleet_pname[k + 1], FLEET_PILOT_LEN);
        fleet_xp[k] = fleet_xp[k + 1];
    }
    fleet_count--;
}

const char *fleet_contracts_check(void) {
    static char m[56];
    for (int i = 0; i < fleet_count; i++) {
        if (fleet_hired[i] && clock_day() > fleet_hired[i]) {
            snprintf(m, sizeof m, "THE HIRED %s'S CONTRACT IS UP. THEY FLY HOME", ship_looks[fleet[i]].name);
            fleet_remove(i);
            return m;
        }
    }
    return 0;
}

void outfit_init(void) {
    credits = START_CREDITS;
    fleet_count = 0;
    memset(sq_order, SQ_HOLD, sizeof sq_order);
    scoop_acc = 0;
    outfit_default_name();
    memset(mod_level, 0, sizeof mod_level);
    fuel_max = fuel = ship_defs[0].fuel_cap;   /* the starting ship, full tank */
    player_layout_def = -1;                    /* a new game: the hull's default fit */
    memset(module_store, 0, sizeof module_store);
    memset(blueprint, 1, sizeof blueprint);
}

void outfit_sync_fuel(const Ship *s) {
    fuel_max = s->def.fuel_cap;
    if (fuel > fuel_max) fuel = fuel_max;
}

const char *mod_name(int mod)   { return names[mod]; }
const char *mod_effect(int mod) { return effects[mod]; }

int mod_next_price(int mod) {
    return mod_level[mod] >= mod_max(mod) ? 0 : level_price(mod, mod_level[mod] + 1);
}

int mod_buy(int mod) {
    int p = mod_next_price(mod);
    if (p == 0 || credits < p) return 0;
    credits -= p;
    mod_level[mod]++;
    return 1;
}

int mod_sell_value(int mod) {
    return mod_level[mod] == 0 ? 0 : level_price(mod, mod_level[mod]) / 2;
}

int mod_sell(int mod) {
    int v = mod_sell_value(mod);
    if (v == 0) return 0;
    credits += v;
    mod_level[mod]--;
    return v;
}

int ship_trade_in_value(int def_index) {
    return (int) (ship_defs[def_index].price / 2);
}

Layout player_layout;
int    player_layout_def = -1;
Layout fleet_layout[FLEET_MAX];
u8     blueprint[MT_COUNT] = { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 };

u8 *outfit_ser(u8 *p, int load) {
    SER(p, load, credits); SER(p, load, pilot_name);
    if (load && ser_version < 6) {                 /* before M15a: no scoop, at index 4 */
        u8 old[6];
        SER(p, load, old);
        memset(mod_level, 0, sizeof mod_level);
        for (int i = 0; i < 6; i++) mod_level[i < MOD_SCOOP ? i : i + 1] = old[i];
    } else if (load && ser_version < 10) {         /* before M17b: no Veyr tech */
        u8 old[7];
        SER(p, load, old);
        memset(mod_level, 0, sizeof mod_level);
        memcpy(mod_level, old, sizeof old);
    } else SER(p, load, mod_level);
    SER(p, load, fuel);
    SER(p, load, fleet); SER(p, load, fleet_stance); SER(p, load, fleet_count);
    SER(p, load, player_layout); SER(p, load, player_layout_def);
    for (int i = 0; i < FLEET_MAX; i++) SER(p, load, fleet_layout[i]);
    SER(p, load, blueprint);
    SER(p, load, fleet_hired);
    SER(p, load, module_store);
    return p;
}

void outfit_compute(int def_index, ShipDef *out, u8 *fire_pct, FitResult *fit, const u8 *hp) {
    *out = ship_defs[def_index];
    if (player_layout_def != def_index) {
        fit_default(def_index, &player_layout);
        player_layout_def = def_index;
    }
    fit_apply(def_index, player_layout.m, player_layout.count, hp, out, fit);
    out->cargo_cap = (u16) (out->cargo_cap * (100 + STORAGE_PCT * mod_level[MOD_STORAGE]) / 100);
    out->turn_rate = (u16) (out->turn_rate * (100 + MANEUVER_PCT * mod_level[MOD_MANEUVER]) / 100);
    *fire_pct = 100;
}

void outfit_refit(Ship *s) {
    int old_hull = s->def.hull_max, old_arc = s->def.shield_arc_max;
    static FitResult fr;
    ship_reset_modules(s);                       /* the layout changed: every module whole */
    outfit_compute(s->def_index, &s->def, &s->fire_pct, &fr, s->mod_hp);
    ship_take_fit(s, &fr);
    outfit_sync_fuel(s);                         /* fuel cells added or removed */
    if (s->def.hull_max > old_hull)
        s->hull += s->def.hull_max - old_hull;
    if (s->def.shield_arc_max > old_arc)
        for (int i = 0; i < ARC_COUNT; i++) s->shield[i] += (s16) (s->def.shield_arc_max - old_arc);
    if (s->hull > s->def.hull_max) s->hull = s->def.hull_max;
    for (int i = 0; i < ARC_COUNT; i++)
        if (s->shield[i] > (s16) s->def.shield_arc_max) s->shield[i] = (s16) s->def.shield_arc_max;
}

/* ------------------------------------------------------------------------ */
/* Fleets: where every ship is, squadrons and their orders (M15a)            */
/* ------------------------------------------------------------------------ */

const char *sq_name(int q) {
    static const char *n[SQUADRONS] = { "WING", "SQUADRON A", "SQUADRON B", "SQUADRON C" };
    return n[q & 3];
}

int sq_count(int q) { int n = 0; for (int i = 0; i < fleet_count; i++) n += fleet_sq[i] == q; return n; }
int sq_lead(int q)  { for (int i = 0; i < fleet_count; i++) if (fleet_sq[i] == q) return i; return -1; }
int sq_where(int q) { int l = sq_lead(q); return l < 0 ? -1 : fleet_sys[l]; }
int fleet_here(int i) { return i >= 0 && i < fleet_count && fleet_sys[i] == galaxy_current(); }

static char fmsg[64];

/* A port sells fuel. Every system has one for now; empty systems come with the full galaxy. */
static int has_port(int sys) { return galaxy_port_open(sys); }

/* Tops up a squadron's ships at a port, as far as the credits go. */
static void sq_refuel(int q) {
    int sys = sq_where(q);
    if (sys < 0 || !has_port(sys)) return;
    for (int i = 0; i < fleet_count; i++) {
        if (fleet_sq[i] != q) continue;
        int cap = ship_defs[fleet[i]].fuel_cap;
        while (fleet_fuel[i] < cap && credits >= FUEL_PRICE) { fleet_fuel[i]++; credits -= FUEL_PRICE; }
    }
}

/* ---- Pilots ---- */

int fleet_rank(int i) { int x = fleet_xp[i]; return x >= 16 ? RANK_ACE : x >= 8 ? RANK_VETERAN : x >= 3 ? RANK_PILOT : RANK_ROOKIE; }
const char *rank_name(int r)  { static const char *n[RANK_COUNT] = { "ROOKIE", "PILOT", "VETERAN", "ACE" }; return n[r & 3]; }
const char *rank_short(int r) { static const char *n[RANK_COUNT] = { "RKE", "PLT", "VET", "ACE" }; return n[r & 3]; }

static char pmsg[64];
const char *fleet_scored(int i) {
    if (i < 0 || i >= fleet_count) return 0;
    int before = fleet_rank(i);
    if (fleet_xp[i] < 999) fleet_xp[i]++;
#ifdef FRONTIER_HOST
    fprintf(stderr, "[pilot] %s scored, %d kills\n", fleet_pname[i], fleet_xp[i]);
#endif
    if (fleet_rank(i) == before) return 0;
    snprintf(pmsg, sizeof pmsg, "%s IS NOW %s %s", fleet_pname[i], fleet_rank(i) == RANK_ACE ? "AN" : "A", rank_name(fleet_rank(i)));
    return pmsg;
}

int def_strength(int def) {
    const ShipDef *d = &ship_defs[def];
    return (int) ((d->hull_max + d->shield_arc_max * 4) * (1 + d->weapon_mounts) / 10);
}
int fleet_strength(int i) { return def_strength(fleet[i]) * (100 + 20 * fleet_rank(i)) / 100; }

const char *mission_name(int m) { static const char *n[MISSION_COUNT] = { "SEND", "TRADE", "ATTACK" }; return n[m % MISSION_COUNT]; }

/* ---- Squadron missions (M15b) ---- */

static u8 doomed[FLEET_MAX];          /* ships lost this hour, removed once the loops are done */

static int sq_power(int q, int armed_only) {
    int t = 0;
    for (int i = 0; i < fleet_count; i++)
        if (fleet_sq[i] == q && !doomed[i] && (!armed_only || ship_defs[fleet[i]].weapon_mounts >= 2)) t += fleet_strength(i);
    return t;
}

/* Each ship of the squadron dies with pct% chance; one always survives if keep_one. Returns the losses. */
static int sq_losses(int q, int pct, int keep_one) {
    int lost = 0, left = 0;
    for (int i = 0; i < fleet_count; i++) if (fleet_sq[i] == q && !doomed[i]) left++;
    for (int i = 0; i < fleet_count; i++) {
        if (fleet_sq[i] != q || doomed[i]) continue;
        if (keep_one && left - lost <= 1) break;
        if (rng_range(100) < pct) { doomed[i] = 1; lost++; }
    }
    return lost;
}

static void sq_learn(int q, int xp) {
    for (int i = 0; i < fleet_count; i++)
        if (fleet_sq[i] == q && !doomed[i]) for (int k = 0; k < xp; k++) fleet_scored(i);
}

static void sq_purge(void) {
    for (int i = fleet_count - 1; i >= 0; i--) if (doomed[i]) { doomed[i] = 0; fleet_remove(i); }
}

/* A trade convoy can meet pirates in poorly policed space. 1: it got through. */
static int convoy_safe(int q, int at) {
    int risk = (3 - (galaxy_sys(at)->security & 3)) * 7;     /* 21% on the Rim, none in the Core */
    if (rng_range(100) >= risk) return 1;
    int pirates = 150 + 100 * (3 - (galaxy_sys(at)->security & 3)) + rng_range(200);
    if (sq_power(q, 1) >= pirates) {
        sq_learn(q, 1);
        snprintf(fmsg, sizeof fmsg, "%s FOUGHT OFF PIRATES AT %s", sq_name(q), galaxy_sys(at)->name);
        return 1;
    }
    int lost = sq_losses(q, 50, 1);
    snprintf(fmsg, sizeof fmsg, "PIRATES HIT %s AT %s: CARGO GONE%s", sq_name(q), galaxy_sys(at)->name,
             lost ? ", A SHIP LOST" : "");
    return 0;
}

static const char *sq_next_leg(int q);

/* The squadron reached its destination: whatever its mission says. */
static const char *sq_arrived(int q, int at) {
    if (sq_mission[q] == MISSION_TRADE) {
        int units = 0;
        for (int i = 0; i < fleet_count; i++) if (fleet_sq[i] == q && !doomed[i]) units += ship_defs[fleet[i]].cargo_cap;
        const char *note = 0;
        if (convoy_safe(q, at)) {
            int c = -1, profit = market_trade_run(sq_home[q], at, units, &c);
            credits += profit;
            if (c >= 0 && profit > 0)
                snprintf(fmsg, sizeof fmsg, "%s SOLD %s AT %s: +%d", sq_name(q), commodities[c].name, galaxy_sys(at)->name, profit);
            else snprintf(fmsg, sizeof fmsg, "%s FOUND NOTHING WORTH CARRYING TO %s", sq_name(q), galaxy_sys(at)->name);
        }
        note = fmsg;
        /* turn round: back the other way, for ever (until you give it new orders) */
        sq_dest[q] = sq_home[q];
        sq_home[q] = (u8) at;
        static char keep[64];
        snprintf(keep, sizeof keep, "%s", note);
        const char *r = sq_next_leg(q);
        return r ? r : keep;
    }
    if (sq_mission[q] == MISSION_ATTACK) {
        sq_mission[q] = MISSION_GO;
        sq_order[q] = SQ_HOLD;
        if (galaxy_sys(at)->owner == OWN_PLAYER) {
            snprintf(fmsg, sizeof fmsg, "%s REACHED %s: IT IS ALREADY YOURS", sq_name(q), galaxy_sys(at)->name);
            return fmsg;
        }
        int F = sq_power(q, 0), D = conquest_defence_strength(at);
        conquest_declare(at);
        if (F >= D) {
            int pct = D * 50 / (F ? F : 1);
            int lost = sq_losses(q, pct, 1);
            sq_learn(q, 3);
            conquest_capture(at);
            snprintf(fmsg, sizeof fmsg, "%s TOOK %s! %d SHIP%s LOST. IT IS YOURS", sq_name(q), galaxy_sys(at)->name,
                     lost, lost == 1 ? "" : "S");
            return fmsg;
        }
        int lost = sq_losses(q, 40 + 40 * (D - F) / (D ? D : 1), 0);
        int left = 0;
        for (int i = 0; i < fleet_count; i++) if (fleet_sq[i] == q && !doomed[i]) left++;
        if (!left) {
            snprintf(fmsg, sizeof fmsg, "%s WAS DESTROYED ATTACKING %s", sq_name(q), galaxy_sys(at)->name);
            return fmsg;
        }
        sq_learn(q, 1);
        static char beat[64];
        snprintf(beat, sizeof beat, "%s BEATEN AT %s: %d LOST, FALLING BACK", sq_name(q), galaxy_sys(at)->name, lost);
        sq_dest[q] = sq_home[q];                      /* the survivors head home */
        const char *r = sq_next_leg(q);
        return r ? r : beat;
    }
    sq_order[q] = SQ_HOLD;
    snprintf(fmsg, sizeof fmsg, "%s ARRIVED AT %s", sq_name(q), galaxy_sys(at)->name);
    return fmsg;
}

/* The next leg of a squadron's journey, from where it is now. */
static const char *sq_next_leg(int q) {
    int at = sq_where(q);
    if (at < 0) { sq_order[q] = SQ_HOLD; return 0; }
    if (at == sq_dest[q]) return sq_arrived(q, at);
    int hop = galaxy_next_hop(at, sq_dest[q]);
    if (hop < 0) {
        sq_order[q] = SQ_HOLD;
        snprintf(fmsg, sizeof fmsg, "%s: NO ROUTE FROM %s", sq_name(q), galaxy_sys(at)->name);
        return fmsg;
    }
    int lead = sq_lead(q), need = galaxy_jump_fuel(at, hop);
    if (fleet_fuel[lead] < need) sq_refuel(q);
    if (fleet_fuel[lead] < need) {
        sq_order[q] = SQ_STRANDED;
        snprintf(fmsg, sizeof fmsg, "%s STRANDED AT %s: NO FUEL", sq_name(q), galaxy_sys(at)->name);
        return fmsg;
    }
    fleet_fuel[lead] -= (u8) need;                  /* only the lead ship burns fuel */
    sq_order[q] = SQ_MOVE;
    sq_next[q] = (u8) hop;
    sq_eta[q] = (u16) galaxy_jump_hours(at, hop);
    if (sq_eta[q] < 1) sq_eta[q] = 1;
    return 0;
}

const char *sq_order_ship(int q, int dest, int mission) {
    if (q < 1 || q >= SQUADRONS || sq_count(q) == 0) return "THAT SQUADRON HAS NO SHIPS";
    if (sq_order[q] == SQ_MOVE) return "IT IS ALREADY IN HYPERSPACE: WAIT FOR IT TO ARRIVE";
    int at = sq_where(q);
    if (mission == MISSION_TRADE && dest == at) return "PICK ANOTHER SYSTEM TO TRADE WITH";
    if (mission == MISSION_TRADE && (!galaxy_port_open(dest) || !galaxy_port_open(at))) return "NO OPEN PORT THERE TO TRADE WITH";
    if (mission == MISSION_TRADE) {
        int cap = 0;
        for (int i = 0; i < fleet_count; i++) if (fleet_sq[i] == q) cap += ship_defs[fleet[i]].cargo_cap;
        if (cap < 10) return "THOSE SHIPS HAVE NO ROOM FOR CARGO";
    }
    if (mission == MISSION_ATTACK && galaxy_sys(dest)->owner == OWN_PLAYER) return "THAT SYSTEM IS ALREADY YOURS";
    if (mission != MISSION_GO && (galaxy_empty(dest) || galaxy_empty(at))) return "EMPTY SPACE: NO PORT TO TRADE WITH OR TAKE";
    if (mission == MISSION_ATTACK && dest == at) {      /* attack where it already is: at once */
        sq_mission[q] = MISSION_ATTACK;
        sq_home[q] = (u8) at;
        const char *m = sq_arrived(q, at);
        sq_purge();
        return m;
    }
    sq_mission[q] = (u8) mission;
    sq_home[q] = (u8) at;
    const char *m = sq_send(q, dest);
    sq_purge();
    return m;
}

const char *sq_send(int q, int dest) {
    if (q < 1 || q >= SQUADRONS || sq_count(q) == 0) return "THAT SQUADRON HAS NO SHIPS";
    if (sq_order[q] == SQ_MOVE) return "IT IS ALREADY IN HYPERSPACE: WAIT FOR IT TO ARRIVE";
    if (dest == sq_where(q)) {
        sq_order[q] = SQ_HOLD;
        snprintf(fmsg, sizeof fmsg, "%s HOLDS %s", sq_name(q), galaxy_sys(dest)->name);
        return fmsg;
    }
    sq_dest[q] = (u8) dest;
    if (sq_mission[q] != MISSION_TRADE) sq_home[q] = (u8) sq_where(q);
    const char *m = sq_next_leg(q);
    if (m) return m;
    if (galaxy_known(dest)) snprintf(fmsg, sizeof fmsg, "%s JUMPS FOR %s", sq_name(q), galaxy_sys(dest)->name);
    else snprintf(fmsg, sizeof fmsg, "%s JUMPS BLIND FOR AN UNCHARTED SYSTEM", sq_name(q));
    return fmsg;
}

const char *fleet_hour(void) {
    const char *m = 0;
    for (int q = 1; q < SQUADRONS; q++) {
        if (sq_count(q) == 0) { sq_order[q] = SQ_HOLD; continue; }
        if (sq_order[q] == SQ_MOVE) {
            if (sq_eta[q] > 0) sq_eta[q]--;
            if (sq_eta[q] == 0) {
                for (int i = 0; i < fleet_count; i++) if (fleet_sq[i] == q) fleet_sys[i] = sq_next[q];
                galaxy_mark_known(sq_next[q]);              /* a squadron charts where it goes */
                const char *r = sq_next_leg(q);
                if (r) m = r;
            }
        } else if (sq_order[q] == SQ_HOLD) {
            sq_refuel(q);                            /* idle at a port: tanks topped up */
        }
    }
    sq_purge();
#ifdef FRONTIER_HOST
    if (m) fprintf(stderr, "[fleet] day %d %02d: %s (credits %d)\n", clock_day(), clock_hour(), m, credits);
#endif
    return m;
}

void fleet_jumped(int from, int to) {
    for (int i = 0; i < fleet_count; i++)
        if (fleet_sq[i] == 0 && fleet_stance[i] != STANCE_HANGAR && fleet_sys[i] == from) fleet_sys[i] = (u8) to;
}

u8 *fleet_ser(u8 *p, int load) {
    if (load && ser_version < 6) {                   /* older saves: everyone with you, tanks full */
        for (int i = 0; i < FLEET_MAX; i++) {
            fleet_sys[i] = (u8) galaxy_current();
            fleet_sq[i] = 0;
            fleet_fuel[i] = i < fleet_count ? (u8) ship_defs[fleet[i]].fuel_cap : 0;
        }
        memset(sq_order, SQ_HOLD, sizeof sq_order);
        memset(sq_mission, MISSION_GO, sizeof sq_mission);
        for (int i = 0; i < FLEET_MAX; i++) { snprintf(fleet_pname[i], FLEET_PILOT_LEN, "PILOT %d", i + 1); fleet_xp[i] = 0; }
        return p;
    }
    SER(p, load, fleet_sys); SER(p, load, fleet_sq); SER(p, load, fleet_fuel);
    SER(p, load, sq_order); SER(p, load, sq_dest); SER(p, load, sq_next); SER(p, load, sq_eta);
    if (load && ser_version < 7) {                   /* M15a saves: no missions, pilots get names now */
        memset(sq_mission, MISSION_GO, sizeof sq_mission);
        for (int i = 0; i < FLEET_MAX; i++) { snprintf(fleet_pname[i], FLEET_PILOT_LEN, "PILOT %d", i + 1); fleet_xp[i] = 0; }
        return p;
    }
    SER(p, load, sq_mission); SER(p, load, sq_home); SER(p, load, fleet_pname); SER(p, load, fleet_xp);
    return p;
}

int scoop_update(int dead_space) {
    int lv = mod_level[MOD_SCOOP];
    if (!lv || !dead_space || fuel >= fuel_max) return 0;
    scoop_acc += lv;
    if (scoop_acc >= SCOOP_FRAMES) { scoop_acc -= SCOOP_FRAMES; fuel++; }
    return 1;
}
