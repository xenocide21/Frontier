#include <stdio.h>
#include <string.h>
#include <psxpad.h>

#include "audio.h"
#include "dock.h"
#include "conquest.h"
#include "veyr.h"
#include "story.h"
#define VEYR_COORDS_PRICE 25000
#include "editor.h"
#include "escort.h"
#include "gfx.h"
#include "clock.h"
#include "faction.h"
#include "galaxy.h"
#include "input.h"
#include "market.h"
#include "outfit.h"
#include "place.h"
#include "mission.h"
#include "lounge.h"
#include "model.h"
#include "save.h"
#include "world.h"

enum { PAGE_MAIN = 0, PAGE_STATUS, PAGE_SHIPYARD, PAGE_TRADE, PAGE_BLACK, PAGE_GUILD, PAGE_LOUNGE, PAGE_SAVE, PAGE_COMMS };
static int editing_hangar;   /* the editor is open on a hangar ship */
enum { TAB_REFIT = 0, TAB_UPGRADES, TAB_BUY, TAB_SELL, TAB_FLEET, TAB_COUNT };
#define NAME_MAX_LEN 16
enum { ITEM_STATUS = 0, ITEM_REPAIR, ITEM_REFUEL, ITEM_SHIPYARD, ITEM_TRADE, ITEM_BLACK, ITEM_LOUNGE, ITEM_GUILD,
       ITEM_COMMS, ITEM_MAP, ITEM_SAVE, ITEM_LAUNCH, ITEM_COUNT };

static const char *item_names[ITEM_COUNT] = {
    "SHIP STATUS", "REPAIR AND REARM", "REFUEL", "SHIPYARD", "TRADING POST", "BLACK MARKET", "LOUNGE", "MERCENARY GUILD",
    "COMMS", "GALAXY MAP", "SAVE GAME", "LAUNCH"
};
/* Items that exist in the menu but are not built yet. */
static const char *item_locked[ITEM_COUNT] = {
    0, 0, 0, 0, 0, 0,
    0,
    0,
    0, 0, 0, 0
};
/* Which place service each item needs (0 = always there). */
static const u8 item_service[ITEM_COUNT] = {
    0, SVC_REPAIR, SVC_REPAIR, SVC_SHIPYARD, SVC_TRADE, SVC_BLACK, SVC_LOUNGE, SVC_GUILD, 0, 0, 0, 0
};

static const char *class_names[] = {
    "SCOUT", "LIGHT FIGHTER", "HEAVY FIGHTER", "CORVETTE", "FRIGATE", "DESTROYER",
    "CRUISER", "TRANSPORT", "FREIGHTER", "MINING VESSEL", "SALVAGE VESSEL"
};

static int active, where, page, selected, msg_timer, anim, yard_sel, yard_tab, trade_sel, trade_top;
#define TRADE_VISIBLE 16         /* rows on the trade page; the list scrolls */
static int renaming, name_pos;
static int rename_fleet = -1;      /* renaming a fleet pilot (M15b), or -1: you */
static char name_buf[PILOT_NAME_MAX];
static char msg[64];
static const Place *here;
static u8 items[ITEM_COUNT];     /* the items this place offers, in menu order */
static int item_count;

void dock_init(void) {
    active = 0;
    msg_timer = 0;
}

int dock_active(void) { return active; }

int dock_check(const Ship *s, int *nav_index) {
    int sx = (int) FROM_FP(s->x), sy = (int) FROM_FP(s->y);
    int found = -1;
    if (world_empty()) return DOCK_NOTHING;

    const NavObject *st = world_nav(NAV_STATION);
    if (approx_len(sx - st->x, sy - st->y) < STATION_RANGE) found = NAV_STATION;

    const NavObject *pl = world_nav(NAV_PLANET);
    if (found < 0 && approx_len(sx - pl->x, sy - pl->y) < pl->radius) found = NAV_PLANET;

    if (found < 0) return DOCK_NOTHING;
    *nav_index = found;
    return ship_speed(s) <= DOCK_MAX_SPEED ? DOCK_READY : DOCK_TOO_FAST;
}

static void say(const char *text) {
    strncpy(msg, text, sizeof msg - 1);
    msg[sizeof msg - 1] = 0;
    msg_timer = 150;
}

void dock_enter(Ship *s, int nav_index) {
    s->docked = 1;
    s->vx = s->vy = 0;
    s->thrust = s->boost = 0;
    where = nav_index;
    page = PAGE_MAIN;
    selected = 0;
    yard_sel = 0;
    yard_tab = TAB_REFIT;
    trade_sel = 0;
    trade_top = 0;
    renaming = 0;
    here = place_for_nav(nav_index);
    item_count = 0;
    for (int i = 0; i < ITEM_COUNT; i++)
        if (item_service[i] == 0 || (here && (here->services & item_service[i])))
            items[item_count++] = (u8) i;
    msg_timer = 0;
    active = 1;
    {
        const char *done = missions_on_dock(galaxy_current(), nav_index);
        if (done) say(done);
    }
}


/* ------------------------------------------------------------------------ */
/* The shipyard: Upgrades, Buy and Sell tabs                                 */
/* ------------------------------------------------------------------------ */

static const char *tab_names[TAB_COUNT] = { "REFIT", "UPGRADES", "BUY", "SELL", "FLEET" };

/* Rows in the SELL tab: the modules, then one line about spare ships. */
/* Syndicate yards (M14b): their own scrap-built ships, and illegal tech. */
/* A Syndicate port keeps its yard and black market after you take it (M14g). */
static int syndicate_here(void) { return galaxy_founder(galaxy_current()) == OWN_SYNDICATE; }
/* The seized hive (M17b): its yard grows Veyr hulls, and fits augmented Veyr tech. */
static int hive_here(void) { return galaxy_current() == VEYR_PRIME && veyr_hive_mine(); }

static int buyable_here(int d) {
    int ors = d >= DEF_SCRAPPER && d <= DEF_MARAUDER;
    if (hive_here()) return veyr_is_ship(d);
    int own = galaxy_sys(galaxy_current())->owner;
    /* Join the navy (M17b): a UFS ally buys its fighters, corvettes and frigates; at 90, its capital ships
     * (M18b: the same rule as the FSA's, no longer the admiral's alone). */
    if (own == OWN_FEDERATION && faction_rep(FAC_FEDERATION) >= REP_ALLY && d >= DEF_TALON && d <= DEF_BASTION) return 1;
    if (own == OWN_FEDERATION && faction_rep(FAC_FEDERATION) >= 90 && (d == DEF_RESOLUTE || d == DEF_SOVEREIGN)) return 1;
    /* ... and the FSA's fighters for its allies, its heavy ships for its heroes. */
    if (own == OWN_REBEL && faction_rep(FAC_REBEL) >= REP_ALLY && d >= DEF_WASP && d <= DEF_DEFIANT) return 1;
    if (own == OWN_REBEL && faction_rep(FAC_REBEL) >= 90 && (d == DEF_REPRISAL || d == DEF_LIBERTY)) return 1;
    return syndicate_here() ? (ship_buyable[d] || ors) : ship_buyable[d];
}

/* A hangar ship somewhere else (M15a): nothing can be done to it from here. */
static const char *elsewhere(int i) {
    static char t[48];
    snprintf(t, sizeof t, "THAT SHIP IS AT %s, NOT HERE", galaxy_sys(fleet_sys[i])->name);
    return t;
}

/* Illegal tech (the last upgrades in the list) is only ever shown at a Syndicate yard. */
static int mods_shown(void) { return hive_here() ? MOD_COUNT : syndicate_here() ? MOD_BIOREGEN : MOD_JAMMER; }

static int flying_def;       /* the hull you fly (set each dock_update) */
static int yard_rows(void) {
    if (yard_tab == TAB_REFIT)    return 1;
    if (yard_tab == TAB_UPGRADES) return mods_shown();
    if (yard_tab == TAB_BUY) {
        int n = 0;
        for (int d = 0; d < SHIP_DEF_COUNT; d++) n += buyable_here(d) && d != flying_def;
        return n;                                       /* every buyable hull but yours */
    }
    if (yard_tab == TAB_FLEET)    return fleet_count > 0 ? fleet_count : 1;
    return mods_shown() + store_rows();             /* upgrades, then stored modules */
}

/* BUY row -> ship definition index (every ship except the one you fly). */
static int yard_ship_for_row(const Ship *s, int row) {
    for (int d = 0; d < SHIP_DEF_COUNT; d++) {
        if (d == s->def_index || !buyable_here(d)) continue;
        if (row-- == 0) return d;
    }
    return -1;
}

static int ship_cost(const Ship *s, int def_index) {
    int price = (int) ship_defs[def_index].price;
    if (veyr_hero_port(galaxy_current())) price -= price / 10;     /* M18b: 10% off for the hero */
    int c = price - ship_trade_in_value(s->def_index);
    return c < 0 ? 0 : c;
}

static void yard_act(Ship *s) {
    if (yard_tab == TAB_REFIT) {
        editor_open(s, &player_layout);
    } else if (yard_tab == TAB_UPGRADES) {
        if (mod_illegal(yard_sel) && !syndicate_here() && !hive_here()) say("ILLEGAL TECH: ONLY A SYNDICATE YARD WILL FIT THAT");
        else if (mod_alien(yard_sel) && !hive_here()) say("ONLY THE HIVE CAN GROW THAT");
        else if (mod_illegal(yard_sel) && !hive_here() && syn_level() < mod_needs_syn(yard_sel)) {
            char t[56];
            snprintf(t, sizeof t, "THEY'LL SELL YOU THAT ONCE YOU'RE %s", syn_level_name(mod_needs_syn(yard_sel)));
            say(t);
        }
        else if (mod_next_price(yard_sel) == 0)  say("ALREADY AT THE TOP LEVEL");
        else if (!mod_buy(yard_sel))             say("NOT ENOUGH CREDITS");
        else { outfit_refit(s);                  say("INSTALLED. SEE SHIP STATUS FOR THE NEW NUMBERS"); }
    } else if (yard_tab == TAB_BUY) {
        int d = yard_ship_for_row(s, yard_sel);
        if (d < 0) return;
        int cost = ship_cost(s, d);
        if (credits < cost) { say("NOT ENOUGH CREDITS"); return; }
        if (cargo_total() > ship_defs[d].cargo_cap) { say("SELL SOME CARGO FIRST: THAT HOLD IS TOO SMALL"); return; }
        credits -= cost;
        ship_set_def(s, d);                      /* a new hull, with your modules fitted */
        outfit_sync_fuel(s);
        yard_sel = 0;
        say("NEW SHIP DELIVERED WITH ITS STANDARD FIT (REFIT TAB TO CHANGE IT)");
    } else if (yard_tab == TAB_FLEET) {
        if (fleet_count == 0 || yard_sel >= fleet_count) { say("NO CLAIMED SHIPS. SALVAGE DISABLED SHIPS TO FIND SOME"); return; }
        int d = fleet[yard_sel];
        if (fleet_hired[yard_sel]) { say("THAT'S A HIRED SHIP: ITS PILOT WON'T HAND IT OVER"); return; }
        if (!fleet_here(yard_sel)) { say(elsewhere(yard_sel)); return; }
        if (cargo_total() > ship_defs[d].cargo_cap) { say("SELL SOME CARGO FIRST: THAT HOLD IS TOO SMALL"); return; }
        fleet[yard_sel] = (u8) s->def_index;     /* your old ship takes its place in the hangar */
        {
            int f = fleet_fuel[yard_sel];        /* each ship keeps its own tank */
            fleet_fuel[yard_sel] = (u8) fuel;
            fuel = f;
            fleet_sq[yard_sel] = 0;
        }
        {                                        /* and each ship keeps its own fit */
            static Layout tmp;
            tmp = player_layout;
            player_layout = fleet_layout[yard_sel];
            fleet_layout[yard_sel] = tmp;
            player_layout_def = d;
        }
        ship_set_def(s, d);                      /* repaired, with its own fit */
        outfit_sync_fuel(s);
        say("SWAPPED SHIPS. YOUR OLD SHIP IS IN THE HANGAR");
    } else {
        if (yard_sel >= mods_shown()) {             /* a module from storage */
            int type, tier;
            store_row(yard_sel - mods_shown(), &type, &tier);
            char t[56];
            snprintf(t, sizeof t, "SOLD A STORED %s T%d: +%d", module_defs[type].name, tier, store_sell(type, tier));
            say(t);
            if (yard_sel >= yard_rows()) yard_sel = yard_rows() - 1;
        } else if (mod_sell(yard_sel) == 0) say("NOTHING FITTED TO SELL");
        else { outfit_refit(s);                  say("SOLD AT HALF PRICE"); }
    }
}

/* Sells the highlighted hangar ship at its trade-in value. */
static void fleet_sell(void) {
    if (fleet_count == 0 || yard_sel >= fleet_count) return;
    if (fleet_hired[yard_sel]) { say("THAT'S A HIRED SHIP. IT ISN'T YOURS TO SELL"); return; }
    if (!fleet_here(yard_sel)) { say(elsewhere(yard_sel)); return; }
    char text[48];
    int v = ship_trade_in_value(fleet[yard_sel]);
    credits += v;
    fleet_remove(yard_sel);
    if (yard_sel >= fleet_count && yard_sel > 0) yard_sel--;
    snprintf(text, sizeof text, "SOLD FOR %d CREDITS", v);
    say(text);
}

static void change_tab(int d) {
    yard_tab = (yard_tab + TAB_COUNT + d) % TAB_COUNT;
    yard_sel = 0;
}

static void refuel(void) {
    int need = fuel_max - fuel;
    char text[64];
    if (need <= 0) { say("THE TANK IS ALREADY FULL"); return; }
    if (veyr_hero_port(galaxy_current())) {                       /* M18b: the hero's fuel is on the house */
        fuel = fuel_max;
        snprintf(text, sizeof text, "REFUELLED %d UNITS. NO CHARGE FOR %s", need, veyr_medal());
        say(text);
        return;
    }
    int can = credits / FUEL_PRICE;
    int n = need < can ? need : can;
    if (n <= 0) { say("NOT ENOUGH CREDITS FOR FUEL"); return; }
    fuel += n;
    credits -= n * FUEL_PRICE;
    snprintf(text, sizeof text, "REFUELLED %d UNITS FOR %d%s", n, n * FUEL_PRICE,
             n < need ? " (TANK NOT FULL: OUT OF CREDITS)" : "");
    say(text);
}

/* ------------------------------------------------------------------------ */
/* The trading post                                                          */
/* ------------------------------------------------------------------------ */

static const char *trade_reason(int result) {
    switch (result) {
    case TRADE_NO_STOCK:   return "OUT OF STOCK";
    case TRADE_NO_ROOM:    return "HOLD FULL";
    case TRADE_NO_CREDITS: return "NOT ENOUGH CREDITS";
    case TRADE_NO_CARGO:   return "NOTHING TO SELL";
    default:               return "";
    }
}

/* Smuggling earns Syndicate standing: selling contraband (double in UFS space),
 * and a little for buying it from them. Returns the change. */
static int smuggle_rep(int units, int selling) {
    if (!commodities[trade_sel].contraband || units <= 0) return 0;
    int d = selling ? (units + 4) / 5 : units / 20;
    if (selling && galaxy_sys(galaxy_current())->owner == OWN_FEDERATION) d *= 2;
    syn_change(d);
    return d;
}

static void trade_buy(const Ship *s, int qty) {
    TradeResult r = market_buy(where, trade_sel, qty, s->def.cargo_cap);
    char text[64];
    smuggle_rep(r.units, 0);
    if (r.units == 0) {
        say(trade_reason(r.result));
    } else if (r.units < qty) {
        snprintf(text, sizeof text, "BOUGHT %d %s FOR %d (%s)", r.units, commodities[trade_sel].name,
                 r.credits, trade_reason(r.result));
        say(text);
    } else {
        snprintf(text, sizeof text, "BOUGHT %d %s FOR %d", r.units, commodities[trade_sel].name, r.credits);
        say(text);
    }
    if (r.units) audio_sfx(SFX_CASH);
}

static void trade_sell(int qty) {
    TradeResult r = market_sell(where, trade_sel, qty);
    char text[64];
    if (r.units == 0) { say(trade_reason(r.result)); return; }
    audio_sfx(SFX_CASH);
    int sr = smuggle_rep(r.units, 1);
    if (sr > 0)
        snprintf(text, sizeof text, "SOLD %d %s FOR %d. SYNDICATE +%d", r.units, commodities[trade_sel].name,
                 r.credits, sr);
    else
    snprintf(text, sizeof text, "SOLD %d %s FOR %d (%s %d)", r.units, commodities[trade_sel].name,
             r.credits, r.profit >= 0 ? "PROFIT +" : "LOSS", r.profit);
    say(text);
}

/* ------------------------------------------------------------------------ */
/* Renaming the pilot (on the status page)                                   */
/* ------------------------------------------------------------------------ */

static const char name_chars[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'-.";

static int name_index(char c) {
    for (int i = 0; name_chars[i]; i++) if (name_chars[i] == c) return i;
    return 0;
}

static void rename_update(void) {
    int n = (int) sizeof name_chars - 1;
    int len = (int) strlen(name_buf);
    int i = name_index(name_buf[name_pos]);

    if (input_pressed(PAD_UP))   name_buf[name_pos] = name_chars[(i + 1) % n];
    if (input_pressed(PAD_DOWN)) name_buf[name_pos] = name_chars[(i + n - 1) % n];
    if (input_pressed(PAD_LEFT) && name_pos > 0) name_pos--;
    if (input_pressed(PAD_RIGHT) && name_pos < (rename_fleet >= 0 ? FLEET_PILOT_LEN - 1 : NAME_MAX_LEN) - 1) {
        name_pos++;
        if (name_pos >= len) {                 /* grow the name by one letter */
            name_buf[name_pos] = 'A';
            name_buf[name_pos + 1] = 0;
        }
    }
    if (input_pressed(PAD_CROSS)) {
        int e = (int) strlen(name_buf);
        while (e > 0 && name_buf[e - 1] == ' ') name_buf[--e] = 0;      /* trim */
        if (rename_fleet >= 0) {
            if (e) { strncpy(fleet_pname[rename_fleet], name_buf, FLEET_PILOT_LEN - 1); fleet_pname[rename_fleet][FLEET_PILOT_LEN - 1] = 0; }
        }
        else if (e == 0) outfit_default_name();
        else { strncpy(pilot_name, name_buf, PILOT_NAME_MAX - 1); pilot_name[PILOT_NAME_MAX - 1] = 0; }
        renaming = 0;
        rename_fleet = -1;
        say("PILOT NAME SAVED");
    }
    if (input_pressed(PAD_CIRCLE)) { renaming = 0; rename_fleet = -1; }
}

/* ------------------------------------------------------------------------ */
/* The black market (Syndicate systems, M13h)                                */
/* ------------------------------------------------------------------------ */

enum { BM_JOBS = 0, BM_DEBT, BM_HOLD, BM_FENCE, BM_WIPE, BM_VEYR, BM_CONTAIN, BM_COUNT };
static const u8 bm_level[BM_COUNT] = { SYN_UNKNOWN, SYN_UNKNOWN, SYN_TRUSTED, SYN_RECOGNIZED, SYN_ESTABLISHED,
                                       SYN_RECOGNIZED, SYN_TRUSTED };
static int guild_sel, guild_syn, guild_tab;
static int lounge_sel, lounge_bet[GAME_COUNT] = { 1, 1 };
static const s32 hold_price[3] = { 6000, 18000, 45000 };
#define WIPE_PRICE 25000
static int black_sel, fence_sel;

static int fence_value(int def) { return (int) (ship_defs[def].price * 3 / 4); }

static void black_update(Ship *s) {
    (void) s;
    if (input_pressed(PAD_UP))   black_sel = (black_sel + BM_COUNT - 1) % BM_COUNT;
    if (input_pressed(PAD_DOWN)) black_sel = (black_sel + 1) % BM_COUNT;
    if (black_sel == BM_FENCE && fleet_count > 0) {
        if (input_pressed(PAD_LEFT))  fence_sel = (fence_sel + fleet_count - 1) % fleet_count;
        if (input_pressed(PAD_RIGHT)) fence_sel = (fence_sel + 1) % fleet_count;
    }
    if (fence_sel >= fleet_count) fence_sel = 0;
    if (input_pressed(PAD_CIRCLE)) { page = PAGE_MAIN; return; }
    if (!input_pressed(PAD_CROSS)) return;

    char text[64];
    if (syn_level() < bm_level[black_sel]) {
        snprintf(text, sizeof text, "THEY WON'T SHOW YOU THAT UNTIL YOU ARE %s", syn_level_name(bm_level[black_sel]));
        say(text);
        return;
    }
    switch (black_sel) {
    case BM_JOBS:
        missions_board_make(galaxy_current(), where, 1);
        guild_syn = 1; guild_sel = 0; page = PAGE_GUILD;
        break;
    case BM_DEBT:
        if (syn_debt <= 0) { say("YOU DON'T OWE THE SYNDICATE ANYTHING. YET"); break; }
        if (credits < syn_debt) { say("NOT ENOUGH CREDITS TO PAY IT OFF"); break; }
        credits -= syn_debt;
        syn_debt = 0;
        syn_change(10);
        say("DEBT PAID. THE LENDER IS ALMOST DISAPPOINTED");
        break;
    case BM_HOLD:
        if (syn_hold_level >= 3) { say("YOUR HOLD IS ALREADY AS HIDDEN AS IT GETS"); break; }
        if (credits < hold_price[syn_hold_level]) { say("NOT ENOUGH CREDITS"); break; }
        credits -= hold_price[syn_hold_level];
        syn_hold_level++;
        snprintf(text, sizeof text, "HIDDEN HOLD FITTED: UFS SCANS MISS %d UNITS OF CONTRABAND", syn_hold_units());
        say(text);
        break;
    case BM_FENCE:
        if (fleet_count == 0) { say("NO SHIPS IN YOUR HANGAR TO FENCE"); break; }
        if (fleet_hired[fence_sel]) { say("FENCE A HIRED SHIP? THE GUILD WOULD HUNT YOU DOWN"); break; }
        if (!fleet_here(fence_sel)) { say(elsewhere(fence_sel)); break; }
        {
            int v = fence_value(fleet[fence_sel]);
            credits += v;
            fleet_remove(fence_sel);
            syn_change(1);
            if (fence_sel >= fleet_count && fence_sel > 0) fence_sel--;
            snprintf(text, sizeof text, "FENCED FOR %d. NO QUESTIONS ASKED", v);
            say(text);
        }
        break;
    case BM_WIPE:
        if (faction_rep(FAC_FEDERATION) >= 0 && faction_rep(FAC_REBEL) >= 0) { say("YOUR RECORDS ARE CLEAN ALREADY"); break; }
        if (credits < WIPE_PRICE) { say("NOT ENOUGH CREDITS"); break; }
        credits -= WIPE_PRICE;
        faction_clear_negative();
        say("A NEW TRANSPONDER CODE AND CLEAN RECORDS. DON'T ASK HOW");
        break;
    case BM_CONTAIN: {                                /* Veyr containment rigs (M17b) */
        int lv = veyr_contain();
        if (veyr_phase() == VP_DORMANT) { say("VEYR TECH? NOBODY'S SEEN A VEYR IN A CENTURY"); break; }
        if (lv >= CONTAIN_MAX) { say("YOUR RIG CAN HOLD A LEVIATHAN. NOTHING BETTER EXISTS"); break; }
        if (credits < veyr_contain_price(lv + 1)) { say("NOT ENOUGH CREDITS"); break; }
        credits -= veyr_contain_price(lv + 1);
        veyr_contain_set(lv + 1);
        static const char *what[CONTAIN_MAX + 1] = { "", "RIG 1: SKITTERS AND MANTISES WILL HOLD STILL FOR YOU",
            "RIG 2: A REAVER WON'T BLOW, MOSTLY", "RIG 3: EVEN A BROODGUARD OR LEVIATHAN" };
        say(what[lv + 1]);
        break;
    }
    default:                                          /* the hive's position (M17a) */
        if (veyr_phase() != VP_INVADING) { say("NOBODY KNOWS WHERE THE VEYR COME FROM. YET"); break; }
        if (veyr_revealed()) { say("YOU HAVE THE COORDINATES ALREADY"); break; }
        if (credits < VEYR_COORDS_PRICE) { say("NOT ENOUGH CREDITS"); break; }
        credits -= VEYR_COORDS_PRICE;
        say(veyr_reveal("SYNDICATE SALVAGERS SOLD YOU THE PATH"));
        break;
    }
}

/* ------------------------------------------------------------------------ */
/* The job board: the mercenary guild, or the Syndicate's (M13j)             */
/* ------------------------------------------------------------------------ */

/* Rows: the offers, then your active contracts, then (guild only) survey data to sell. */
static int guild_rows(void) {
    if (guild_tab) return hire_count() > 0 ? hire_count() : 1;
    return missions_offer_count() + missions_active_count() + (!guild_syn && missions_discoveries_unsold() ? 1 : 0);
}

static void guild_update(Ship *s) {
    int n = guild_rows();
    if (n > 0) {
        if (input_pressed(PAD_UP))   guild_sel = (guild_sel + n - 1) % n;
        if (input_pressed(PAD_DOWN)) guild_sel = (guild_sel + 1) % n;
    }
    if (guild_sel >= n) guild_sel = n > 0 ? n - 1 : 0;
    if (input_pressed(PAD_CIRCLE)) { page = guild_syn ? PAGE_BLACK : PAGE_MAIN; return; }
    if (!guild_syn && input_pressed(PAD_L1 | PAD_R1 | PAD_LEFT | PAD_RIGHT)) { guild_tab ^= 1; guild_sel = 0; return; }
    if (guild_tab) {
        if (input_pressed(PAD_CROSS) && hire_count() > 0) {
            const char *r = hire_take(guild_sel);
            if (r) say(r);
        }
        return;
    }
    int no = missions_offer_count(), na = missions_active_count();
    if (input_pressed(PAD_CROSS)) {
        if (guild_sel < no) {
            const char *r = missions_accept(guild_sel, s->def.cargo_cap);
            if (r) say(r);
        } else if (guild_sel >= no + na && guild_rows() > no + na) {
            char t[48];
            snprintf(t, sizeof t, "SURVEY DATA SOLD FOR %d", missions_sell_data());
            say(t);
        }
    }
    if (input_pressed(PAD_SQUARE) && guild_sel >= no && guild_sel < no + na) {
        missions_abandon(guild_sel - no);
        say("CONTRACT ABANDONED. THEY WON'T FORGET IT SOON");
    }
}

/* ------------------------------------------------------------------------ */
/* Saving (M14f): three manual slots; the autosave happens on docking        */
/* ------------------------------------------------------------------------ */

static SaveInfo save_slots[SAVE_SLOTS];
static int save_sel = 1, save_confirm;

/* Every time you dock: quietly saved to the autosave slot. */
void dock_autosave(const Ship *s, int nav_index) {
    int r = save_write(SAVE_AUTO, s, nav_index);
    if (r != SAVE_OK && r != SAVE_NO_CARD) say("AUTOSAVE FAILED: CHECK THE MEMORY CARD");
}

static void save_page_update(Ship *s) {
    if (input_pressed(PAD_UP))   { save_sel = save_sel > 1 ? save_sel - 1 : SAVE_SLOTS - 1; save_confirm = 0; }
    if (input_pressed(PAD_DOWN)) { save_sel = save_sel < SAVE_SLOTS - 1 ? save_sel + 1 : 1; save_confirm = 0; }
    if (input_pressed(PAD_CIRCLE)) { page = PAGE_MAIN; return; }
    if (!input_pressed(PAD_CROSS)) return;
    if (save_slots[save_sel].used && !save_confirm) {
        save_confirm = 1;
        say("X AGAIN TO SAVE OVER IT");
        return;
    }
    save_confirm = 0;
    int r = save_write(save_sel, s, where);
    say(save_error(r));
    if (r == SAVE_OK) save_scan(save_slots);
}

/* ------------------------------------------------------------------------ */
/* The lounge (M14b): rows are the stranger's offer, the jobs, then the games */
/* ------------------------------------------------------------------------ */

static int lounge_rows(void) { return lounge_has_offer() + missions_offer_count() + GAME_COUNT; }

static void lounge_update(Ship *s) {
    int n = lounge_rows();
    if (input_pressed(PAD_UP))   lounge_sel = (lounge_sel + n - 1) % n;
    if (input_pressed(PAD_DOWN)) lounge_sel = (lounge_sel + 1) % n;
    if (lounge_sel >= n) lounge_sel = n - 1;
    if (input_pressed(PAD_CIRCLE)) { page = PAGE_MAIN; return; }
    int off = lounge_has_offer(), jobs = missions_offer_count();
    int game = lounge_sel - off - jobs;
    if (game >= 0) {
        if (input_pressed(PAD_LEFT))  lounge_bet[game] = (lounge_bet[game] + 3) % 4;
        if (input_pressed(PAD_RIGHT)) lounge_bet[game] = (lounge_bet[game] + 1) % 4;
    }
    if (!input_pressed(PAD_CROSS)) return;
    const char *r = 0;
    if (lounge_sel < off) r = lounge_take_offer(s->def.cargo_cap);
    else if (game < 0)    r = missions_accept(lounge_sel - off, s->def.cargo_cap);
    else                  r = lounge_gamble(game, lounge_bet[game]);
    if (r) say(r);
}

/* ------------------------------------------------------------------------ */
/* COMMS (M18a): the three story lines, and a contact's briefing             */
/* ------------------------------------------------------------------------ */

static int comms_sel, comms_brief = -1, comms_confirm;

static void comms_update(Ship *s) {
    if (comms_brief >= 0) {                              /* reading a briefing */
        if (input_pressed(PAD_CIRCLE)) { comms_brief = -1; comms_confirm = 0; return; }
        if (!input_pressed(PAD_CROSS)) return;
        if (story_offer_warning(comms_brief) && !comms_confirm) { comms_confirm = 1; return; }
        say(story_accept(comms_brief, galaxy_current(), where, s->def.cargo_cap));
        comms_brief = -1; comms_confirm = 0;
        return;
    }
    if (input_pressed(PAD_UP))   comms_sel = (comms_sel + TRACK_COUNT - 1) % TRACK_COUNT;
    if (input_pressed(PAD_DOWN)) comms_sel = (comms_sel + 1) % TRACK_COUNT;
    if (input_pressed(PAD_CIRCLE)) { page = PAGE_MAIN; return; }
    if (input_pressed(PAD_CROSS)) {
        if (story_offer_here(comms_sel, galaxy_current())) { comms_brief = comms_sel; comms_confirm = 0; }
        else say(story_status(comms_sel, galaxy_current()));
    }
}

int dock_update(Ship *s) {
    flying_def = s->def_index;
    audio_menu();                                      /* M19a: clicks */
    {
        const char *fm = faction_take_message();       /* a mission or sale just made you pick a side */
        if (fm) say(fm);
    }
    if (msg_timer > 0) msg_timer--;
    anim++;

    if (page == PAGE_STATUS) {
        if (renaming) { rename_update(); return DOCK_EVENT_NONE; }
        if (input_pressed(PAD_SQUARE)) {
            strncpy(name_buf, pilot_name, PILOT_NAME_MAX - 1);
            name_buf[PILOT_NAME_MAX - 1] = 0;
            name_pos = (int) strlen(name_buf);
            if (name_pos > NAME_MAX_LEN - 1) name_pos = NAME_MAX_LEN - 1;
            if (name_pos > 0) name_pos--;
            renaming = 1;
        } else if (input_pressed(PAD_CIRCLE | PAD_CROSS)) {
            page = PAGE_MAIN;
        }
        return DOCK_EVENT_NONE;
    }

    if (page == PAGE_BLACK) {
        black_update(s);
        return DOCK_EVENT_NONE;
    }
    if (page == PAGE_GUILD) {
        guild_update(s);
        return DOCK_EVENT_NONE;
    }
    if (page == PAGE_LOUNGE) {
        lounge_update(s);
        return DOCK_EVENT_NONE;
    }
    if (page == PAGE_COMMS) {
        comms_update(s);
        return DOCK_EVENT_NONE;
    }
    if (page == PAGE_SAVE) {
        save_page_update(s);
        return DOCK_EVENT_NONE;
    }

    if (page == PAGE_TRADE) {
        int big = input_held(PAD_R1) ? 10 : 1;
        if (input_pressed(PAD_UP))   trade_sel = (trade_sel + COMM_COUNT - 1) % COMM_COUNT;
        if (input_pressed(PAD_DOWN)) trade_sel = (trade_sel + 1) % COMM_COUNT;
        if (trade_sel < trade_top) trade_top = trade_sel;
        if (trade_sel >= trade_top + TRADE_VISIBLE) trade_top = trade_sel - TRADE_VISIBLE + 1;
        if (input_pressed(PAD_SELECT)) {
            if (credits < CHART_PRICE) {
                say("NOT ENOUGH CREDITS FOR A CHART");
            } else {
                int fresh = galaxy_chart();
                if (fresh == 0) {
                    say("NOTHING NEW TO CHART AROUND HERE");
                } else {
                    char t[48];
                    credits -= CHART_PRICE;
                    world_set_system(galaxy_current());
                    snprintf(t, sizeof t, "CHART BOUGHT: %d NEW SYSTEM%s ON YOUR MAP", fresh, fresh == 1 ? "" : "S");
                    say(t);
                }
            }
        }
        if (input_pressed(PAD_CROSS))    trade_buy(s, big);
        if (input_pressed(PAD_SQUARE))   trade_sell(big);
        if (input_pressed(PAD_TRIANGLE)) trade_sell(cargo[trade_sel] ? cargo[trade_sel] : 1);
        if (input_pressed(PAD_CIRCLE))   page = PAGE_MAIN;
        return DOCK_EVENT_NONE;
    }

    if (page == PAGE_SHIPYARD && editor_active()) {
        editor_update();
        if (!editor_active() && editing_hangar) { escort_refit_all(); editing_hangar = 0; }
        return DOCK_EVENT_NONE;
    }
    if (page == PAGE_SHIPYARD && renaming) { rename_update(); return DOCK_EVENT_NONE; }
    if (page == PAGE_SHIPYARD) {
        int rows = yard_rows();
        if (input_pressed(PAD_SELECT) && yard_tab == TAB_FLEET && yard_sel < fleet_count) {   /* rename its pilot */
            rename_fleet = yard_sel;
            strncpy(name_buf, fleet_pname[yard_sel], FLEET_PILOT_LEN - 1);
            name_buf[FLEET_PILOT_LEN - 1] = 0;
            name_pos = (int) strlen(name_buf);
            if (name_pos > FLEET_PILOT_LEN - 2) name_pos = FLEET_PILOT_LEN - 2;
            if (name_pos > 0) name_pos--;
            renaming = 1;
            return DOCK_EVENT_NONE;
        }
        if (input_pressed(PAD_LEFT | PAD_L1))  change_tab(-1);
        if (input_pressed(PAD_RIGHT | PAD_R1)) change_tab(1);
        rows = yard_rows();
        if (input_pressed(PAD_UP))   yard_sel = (yard_sel + rows - 1) % rows;
        if (input_pressed(PAD_DOWN)) yard_sel = (yard_sel + 1) % rows;
        if (input_pressed(PAD_CROSS))  yard_act(s);
        if (input_pressed(PAD_SQUARE) && yard_tab == TAB_FLEET) fleet_sell();
        if (input_pressed(PAD_TRIANGLE) && yard_tab == TAB_FLEET && yard_sel < fleet_count && !fleet_here(yard_sel))
            say(elsewhere(yard_sel));
        else if (input_pressed(PAD_TRIANGLE) && yard_tab == TAB_FLEET && yard_sel < fleet_count) {
            static Ship hangar_ship;             /* a stand-in for the hangar ship being refitted */
            ship_init(&hangar_ship, fleet[yard_sel], TEAM_ALLY);
            ship_set_layout(&hangar_ship, &fleet_layout[yard_sel]);
            editor_open(&hangar_ship, &fleet_layout[yard_sel]);
            editing_hangar = 1;
        }
        if (input_pressed(PAD_CIRCLE)) page = PAGE_MAIN;
        return DOCK_EVENT_NONE;
    }

    if (input_pressed(PAD_UP))   selected = (selected + item_count - 1) % item_count;
    if (input_pressed(PAD_DOWN)) selected = (selected + 1) % item_count;

    int item = items[selected];
    if (input_pressed(PAD_CROSS)) {
        if (item_locked[item]) {
            say(item_locked[item]);
        } else if (item == ITEM_STATUS) {
            page = PAGE_STATUS;
        } else if (item == ITEM_SHIPYARD) {
            page = PAGE_SHIPYARD;
        } else if (item == ITEM_TRADE) {
            page = PAGE_TRADE;
        } else if (item == ITEM_GUILD) {
            missions_board_make(galaxy_current(), where, BOARD_GUILD);
            hire_make(galaxy_current(), where);
            guild_syn = 0; guild_sel = 0; guild_tab = 0; page = PAGE_GUILD;
        } else if (item == ITEM_COMMS) {
            comms_sel = 0; comms_brief = -1; comms_confirm = 0; page = PAGE_COMMS;
        } else if (item == ITEM_LOUNGE) {
            lounge_make(galaxy_current(), where);
            missions_board_make(galaxy_current(), where, BOARD_LOUNGE);
            lounge_sel = 0; page = PAGE_LOUNGE;
        } else if (item == ITEM_BLACK) {
            if (syn_rep() <= SYN_MARKED && galaxy_sys(galaxy_current())->owner != OWN_PLAYER)
                say("NOBODY HERE WILL DEAL WITH A PIRATE KILLER");     /* in your own port they must */
            else { page = PAGE_BLACK; black_sel = 0; fence_sel = 0; }
        } else if (item == ITEM_REPAIR) {
            while (ship_repair(s)) { }
            say("SHIP REPAIRED AND REARMED");
        } else if (item == ITEM_REFUEL) {
            refuel();
        } else if (item == ITEM_SAVE) {
            save_scan(save_slots);
            save_sel = 1; save_confirm = 0; page = PAGE_SAVE;
        } else if (item == ITEM_MAP) {
            galaxy_map_open();
        } else if (item == ITEM_LAUNCH) {
            s->docked = 0;
            active = 0;
            return DOCK_EVENT_LAUNCH;
        }
    }
    if (input_pressed(PAD_CIRCLE) || input_pressed(PAD_START)) {
        /* Backing out of the main menu launches, as in most space games. */
        s->docked = 0;
        active = 0;
        return DOCK_EVENT_LAUNCH;
    }
    return DOCK_EVENT_NONE;
}

/* ------------------------------------------------------------------------ */
/* Drawing: the dock screen fills the display (inside the TV-safe margin)    */
/* ------------------------------------------------------------------------ */

#define PANEL_X 16
#define PANEL_Y 16
#define PANEL_W 608
#define PANEL_H 448
#define LEFT    (PANEL_X + 28)
#define TOP     (PANEL_Y + 84)           /* first content row */
#define ART_W   236
#define ART_H   144
#define BTN_W   300
#define BTN_H    (item_count > 10 ? 19 : 22)
#define BTN_PITCH (item_count > 10 ? 22 : 26)   /* M18a: COMMS makes some menus long */

#define TEXT_LAYER  0
#define PANEL_LAYER 1     /* behind the text and the picture */

static int speed_pxs(const ShipDef *d) { return (int) (((s32) d->max_speed * FRAMES_PER_SECOND) >> FP_SHIFT); }

static void line1(int y, int r, int g, int b, const char *text) {
    gfx_text_big(LEFT, y, TEXT_LAYER, 1, r, g, b, text);
}

static void hint(const char *text) {
    line1(PANEL_Y + PANEL_H - 26, 150, 160, 180, text);
}

/* Ship status: two cards. Left, the ship (health bars and performance);
 * right, the pilot (name, standing, cargo, fuel, missiles and modules). */
#define CARD_W   268
#define CARD_H   300
#define CARD2_X  (LEFT + CARD_W + 16)

static void small(int x, int y, int r, int g, int b, const char *t) {
    gfx_text_big(x, y, TEXT_LAYER, 1, r, g, b, t);
}

static void meter_row(int x, int y, const char *label, int value, int max, int r, int g, int b) {
    small(x, y, 230, 235, 245, label);
    gfx_bar(x + 136, y, 116, 8, TEXT_LAYER, value, max, r, g, b);   /* labels up to 16 characters */
}

static void draw_status(const Ship *s) {
    const ShipDef *d = &s->def;
    char line[64];
    static const char *arc_names[ARC_COUNT] = { "FRONT", "RIGHT", "BACK", "LEFT" };
    static const char *sys_names[SYS_COUNT] = { "ENGINES", "REACTOR", "SHIELD GEN", "WEAPONS" };

    /* --- Left card: the ship --- */
    int x = LEFT + 10, y = TOP + 8;
    gfx_text_big(x, y, TEXT_LAYER, 2, 255, 230, 80, ship_looks[s->def_index].name);
    small(x, y + 20, 150, 170, 200, class_names[d->cls]);
    y += 40;
    snprintf(line, sizeof line, "HULL %d/%d", (int) s->hull, (int) d->hull_max);
    meter_row(x, y, line, (int) s->hull, d->hull_max, 230, 120, 90);          y += 14;
    for (int i = 0; i < ARC_COUNT; i++) {
        snprintf(line, sizeof line, "SHLD %s %d", arc_names[i], s->shield[i]);
        meter_row(x, y, line, s->shield[i], d->shield_arc_max, 110, 180, 255); y += 14;
    }
    for (int i = 0; i < SYS_COUNT; i++) {
        snprintf(line, sizeof line, "%s %d", sys_names[i], s->sys[i]);
        int v = s->sys[i];
        meter_row(x, y, line, v, 100, v > 60 ? 120 : 255, v > 60 ? 220 : v > 25 ? 200 : 90, 120);
        y += 14;
    }
    y += 10;
    small(x, y, 255, 200, 90, "PERFORMANCE");                                 y += 14;
    snprintf(line, sizeof line, "TOP SPEED %d PX/S  BOOST X1.5", speed_pxs(d));
    small(x, y, 230, 235, 245, line);                                         y += 14;
    snprintf(line, sizeof line, "ACCEL %d   TURN %d", d->accel, d->turn_rate);
    small(x, y, 230, 235, 245, line);                                         y += 14;
    snprintf(line, sizeof line, "RETRO-THRUSTERS %s", d->brakes ? "YES" : "NO (DRIFTS)");
    small(x, y, 230, 235, 245, line);                                         y += 14;
    snprintf(line, sizeof line, "SHIELD RECHARGE %d/S", d->shield_recharge);
    small(x, y, 230, 235, 245, line);                                         y += 14;
    snprintf(line, sizeof line, "GUNS %d  COOLDOWN %d%%", d->weapon_mounts, s->fire_pct);
    small(x, y, 230, 235, 245, line);
    gfx_card(LEFT, TOP, CARD_W, CARD_H, TEXT_LAYER);

    /* --- Right card: the pilot --- */
    x = CARD2_X + 10;
    y = TOP + 8;
    small(x, y, 150, 170, 200, "PILOT");
    if (renaming) {
        gfx_text_big(x, y + 12, TEXT_LAYER, 2, 255, 230, 80, name_buf);
        if ((anim >> 4) & 1)
            gfx_rect(x + name_pos * 16, y + 29, 14, 3, TEXT_LAYER, 255, 230, 80);
    } else {
        gfx_text_big(x, y + 12, TEXT_LAYER, 2, 230, 235, 245, pilot_name);
    }
    {
        static const char *dn[DIFF_COUNT] = { "EASY", "NORMAL", "HARD" };
        char pt[16];
        save_play_time(pt, sizeof pt, play_frames);
        if (veyr_title()) snprintf(line, sizeof line, "PLAYED %s   %s", pt, veyr_title());
        else snprintf(line, sizeof line, "PLAYED %s   VEYR: %s", pt, dn[game_difficulty % DIFF_COUNT]);
        small(x, y + 32, 150, 170, 200, line);
    }
    y += 46;

    small(x, y, 255, 200, 90, "STANDING (CENTRE = NEUTRAL)");                                   y += 14;
    snprintf(line, sizeof line, "UFS %s", faction_standing(FAC_FEDERATION));
    meter_row(x, y, line, faction_rep(FAC_FEDERATION) + 100, 200, 90, 160, 255); y += 14;
    snprintf(line, sizeof line, "FSA %s", faction_standing(FAC_REBEL));
    meter_row(x, y, line, faction_rep(FAC_REBEL) + 100, 200, 230, 80, 210);      y += 14;
    snprintf(line, sizeof line, "ORS %s", syn_level_name(syn_level()));
    meter_row(x, y, line, syn_rep() + 100, 200, 200, 140, 90);                    y += 16;

    snprintf(line, sizeof line, "CARGO %d/%d", cargo_total(), d->cargo_cap);
    meter_row(x, y, line, cargo_total(), d->cargo_cap, 255, 200, 90);          y += 14;
    snprintf(line, sizeof line, "FUEL %d/%d", fuel, fuel_max);
    meter_row(x, y, line, fuel, fuel_max, 150, 130, 255);                       y += 14;
    {
        int held = 0;                                       /* M14g: conquered systems */
        for (int i = 0; i < GALAXY_SYSTEMS; i++)
            if (galaxy_sys(i)->owner == OWN_PLAYER) held++;
        if (held) snprintf(line, sizeof line, "CREW %d HANGAR %d SYSTEMS %d", d->crew_cap, fleet_count, held);
        else      snprintf(line, sizeof line, "CREW %d   HANGAR %d SHIPS", d->crew_cap, fleet_count);
    }
    small(x, y, 230, 235, 245, line);                                           y += 16;

    snprintf(line, sizeof line, "MISSILES   TURRETS %d", s->loadout->turrets);
    small(x, y, 255, 200, 90, line);                                            y += 14;
    for (int t = 0; t < MSL_COUNT; t++) {
        int mx = ship_missile_max(s, t);
        snprintf(line, sizeof line, "%s %d/%d", missile_name(t), s->msl[t], mx);
        if (mx) meter_row(x, y, line, s->msl[t], mx, 255, 190, 70);
        else    small(x, y, 120, 125, 140, line);
        y += 14;
    }
    y += 2;

    snprintf(line, sizeof line, "MODULES %d  DAMAGED %d  LOST %d", s->fit_count, ship_modules_hurt(s) - ship_modules_lost(s),
             ship_modules_lost(s));
    small(x, y, 255, 200, 90, line);                                           y += 14;
    int mshown = 0;
    for (int m = 0; m < MOD_COUNT; m++) mshown += !((mod_illegal(m) || mod_alien(m)) && mod_level[m] == 0);
    int mpitch = mshown > 5 ? 11 : 14;                         /* squeeze the list when illegal tech is fitted */
    for (int m = 0; m < MOD_COUNT; m++) {
        if ((mod_illegal(m) || mod_alien(m)) && mod_level[m] == 0) continue;   /* illegal or Veyr tech: only listed once fitted */
        small(x, y, 230, 235, 245, mod_name(m));
        for (int l = 0; l < mod_max(m); l++) {
            int on = l < mod_level[m];
            gfx_rect(x + 150 + l * 10, y, 7, 7, TEXT_LAYER, on ? 255 : 30, on ? 220 : 38, on ? 80 : 64);
        }
        y += mpitch;
    }
    gfx_card(CARD2_X, TOP, CARD_W, CARD_H, TEXT_LAYER);

    if (renaming) hint("UP DOWN: LETTER   LEFT RIGHT: MOVE OR ADD   X: SAVE   O: CANCEL");
    else          hint("SQUARE: RENAME PILOT   X OR O: BACK");
}

static int art_ship;
static void draw_main(void) {
    for (int i = 0; i < item_count; i++) {
        int item = items[i];
        int y = TOP + i * BTN_PITCH;
        int st = i == selected ? BTN_SELECTED : item_locked[item] ? BTN_DISABLED : BTN_NORMAL;
        gfx_button(LEFT, y, BTN_W, BTN_H, TEXT_LAYER, st, 2, item_names[item]);
    }

    /* The interior picture and the description of the place. */
    place_set_ship(art_ship);
    place_draw_art(here, PANEL_X + PANEL_W - 28 - ART_W, TOP, ART_W, ART_H, TEXT_LAYER, anim);
    if (here)
        for (int i = 0; i < PLACE_DESC_LINES; i++)
            line1(TOP + (item_count * BTN_PITCH > ART_H ? item_count * BTN_PITCH : ART_H) + 12 + i * 14, 170, 190, 215, here->desc[i]);

    hint("UP DOWN: CHOOSE   X: SELECT   O OR START: LAUNCH");
}

/* Trading post: one thin button per commodity. Selected = yellow, held = cyan. */
#define TRADE_ROW_H     15
#define TRADE_ROW_PITCH 17
static void draw_hires(void) {
    small(LEFT, TOP - 8, 255, 200, 90, "ESCORT PILOTS FOR HIRE (X: HIRE)   L1 R1: CONTRACTS");
    int y = TOP + 8;
    if (hire_count() == 0) line1(y, 150, 170, 200, "NOBODY'S LOOKING FOR WORK TODAY.");
    for (int i = 0; i < hire_count(); i++) {
        gfx_button(LEFT, y, PANEL_W - 56, 20, TEXT_LAYER, guild_sel == i ? BTN_SELECTED : BTN_NORMAL, 1, hire_line(i));
        y += 26;
    }
    y += 12;
    line1(y, 150, 170, 200, "A HIRED SHIP JOINS YOUR FLEET (START > FLEET SETS ITS STANCE)");  y += 14;
    line1(y, 150, 170, 200, "AND FLIES HOME WHEN ITS DAYS ARE UP. YOU CAN'T SELL, FENCE OR FLY IT.");  y += 14;
    line1(y, 150, 170, 200, "ONLY 3 ESCORTS FLY WITH YOU AT ONCE.");
    hint("UP DOWN: CHOOSE   X: HIRE   L1 R1: CONTRACTS   O: BACK");
}

/* Two lines from a "line one|line two" string. */
static int two_lines(int y, int r, int g, int b, const char *t) {
    char a[72];
    const char *bar = strchr(t, '|');
    int n = bar ? (int) (bar - t) : (int) strlen(t);
    if (n > 71) n = 71;
    memcpy(a, t, (size_t) n);
    a[n] = 0;
    line1(y, r, g, b, a);
    if (bar) { line1(y + 12, r, g, b, bar + 1); return y + 26; }
    return y + 14;
}

static void draw_save_page(void) {
    static const char *dn[DIFF_COUNT] = { "EASY", "NORMAL", "HARD" };
    char line[72], pt[16];
    small(LEFT, TOP - 8, 255, 200, 90, "CHOOSE A SLOT. YOU AUTOSAVE EVERY TIME YOU DOCK.");
    for (int i = 0; i < SAVE_SLOTS; i++) {
        const SaveInfo *si = &save_slots[i];
        int y = TOP + 10 + i * 46;
        if (si->used) snprintf(line, sizeof line, "%-8s %s", save_slot_name(i), si->name);
        else          snprintf(line, sizeof line, "%-8s EMPTY", save_slot_name(i));
        gfx_button(LEFT, y, PANEL_W - 56, 22, TEXT_LAYER,
                   i == 0 ? BTN_DISABLED : i == save_sel ? BTN_SELECTED : BTN_NORMAL, 2, line);
        if (si->used) {
            save_play_time(pt, sizeof pt, si->play_frames);
            snprintf(line, sizeof line, "DAY %d   PLAYED %s   %s   %s   %d CR", si->day, pt,
                     si->sys < GALAXY_SYSTEMS ? galaxy_sys(si->sys)->name : "?", dn[si->difficulty % DIFF_COUNT],
                     (int) si->credits);
            small(LEFT + 16, y + 26, 150, 200, 160, line);
        }
    }
    save_play_time(pt, sizeof pt, play_frames);
    snprintf(line, sizeof line, "THIS GAME: DAY %d, PLAYED %s", clock_day(), pt);
    line1(TOP + 10 + SAVE_SLOTS * 46 + 6, 220, 225, 235, line);
    hint("UP DOWN: CHOOSE   X: SAVE   O: BACK");
}

/* Wraps a long line at a space near `width` characters; returns the y after it. */
static int wrapped(int y, int width, int r, int g, int b, const char *t) {
    char ln[80];
    while (*t) {
        int n = (int) strlen(t);
        if (n > width) { n = width; while (n > 0 && t[n] != ' ') n--; if (n == 0) n = width; }
        if (n > 79) n = 79;
        memcpy(ln, t, (size_t) n); ln[n] = 0;
        line1(y, r, g, b, ln);
        y += 14;
        t += n;
        while (*t == ' ') t++;
    }
    return y;
}

static void draw_comms(void) {
    char t[80];
    int y = TOP - 8;
    if (comms_brief >= 0) {
        int k = comms_brief;
        small(LEFT, y, 255, 200, 90, story_contact(k));                                      y += 18;
        gfx_text_big(LEFT, y, TEXT_LAYER, 2, 230, 235, 245, story_offer_title(k));             y += 30;
        const char *ls[6];
        int n = story_offer_lines(k, ls);
        for (int i = 0; i < n; i++) { line1(y, 200, 210, 225, ls[i]); y += 16; }
        y += 8;
        snprintf(t, sizeof t, "PAYS %d CREDITS", story_offer_pay(k));
        line1(y, 255, 230, 80, t);                                                            y += 22;
        const char *w = story_offer_warning(k);
        if (w) {
            y = wrapped(y, 66, 255, 90, 70, w);
            y += 6;
            line1(y, 255, 90, 70, comms_confirm ? "ARE YOU SURE? X AGAIN TO SWEAR. O TO WALK AWAY."
                                                : "X: SWEAR IT   O: NOT YET");
        } else line1(y, 120, 220, 160, "X: TAKE THE JOB   O: BACK");
        hint(w ? "THIS CANNOT BE UNDONE" : "THE JOB GOES ON YOUR CONTRACT LIST");
        return;
    }
    small(LEFT, y, 255, 200, 90, "THE PEOPLE WHO WANT YOU ON THEIR SIDE");                    y += 16;
    for (int k = 0; k < TRACK_COUNT; k++) {
        int offer = story_offer_here(k, galaxy_current());
        snprintf(t, sizeof t, "%s%s", story_track_name(k), offer ? "   NEW ORDERS" : "");
        gfx_button(LEFT, y, PANEL_W - 56, 22, TEXT_LAYER, comms_sel == k ? BTN_SELECTED : BTN_NORMAL, 2, t);
        small(LEFT + 8, y + 26, 150, 170, 200, story_contact(k));
        small(LEFT + 8, y + 38, offer ? 120 : 170, offer ? 230 : 180, offer ? 150 : 200, story_status(k, galaxy_current()));
        y += 62;
    }
    y += 6;
    if (veyr_title()) {                                  /* M18b: after the war, the next one */
        snprintf(t, sizeof t, "%s, BEARER OF %s", veyr_title(), veyr_medal());
        line1(y, 255, 205, 60, t);
        line1(y + 14, conquest_war_won() ? 120 : 230, conquest_war_won() ? 230 : 200, conquest_war_won() ? 140 : 120,
              conquest_war_status());
    } else {
        if (story_sworn() >= 0) snprintf(t, sizeof t, "YOU ARE SWORN TO THE %s.", story_track_name(story_sworn()));
        else snprintf(t, sizeof t, "EACH SIDE'S JOBS RAISE YOU WITH THEM AND COST YOU WITH THE REST.");
        line1(y, 200, 205, 215, t);
        line1(y + 14, 170, 180, 200, "IN TIME EACH ASKS YOU TO SWEAR TO THEM. YOU CAN SWEAR ONLY ONCE.");
    }
    hint("UP DOWN: CHOOSE   X: HEAR THEM OUT   O: BACK");
}

static void draw_lounge(void) {
    static const char *times[4] = { "SMALL HOURS", "MORNING", "AFTERNOON", "EVENING" };
    char t[72], d[72];
    int y = TOP - 10;
    snprintf(t, sizeof t, "THE LOUNGE   DAY %d, %s", clock_day(), times[clock_hour() / 6]);
    small(LEFT, y, 255, 200, 90, t);                                                  y += 14;
    for (int i = 0; i < LOUNGE_RUMOURS && lounge_rumour(i); i++)
        y = two_lines(y, 200, 210, 225, lounge_rumour(i));
    y += 2;
    y = two_lines(y, 170, 225, 180, lounge_encounter());
    int row = 0;
    if (lounge_has_offer()) {
        gfx_button(LEFT, y, PANEL_W - 56, 15, TEXT_LAYER, lounge_sel == row ? BTN_SELECTED : BTN_NORMAL, 1, lounge_offer_line());
        y += 19; row++;
    }
    y += 2;
    small(LEFT, y, 255, 200, 90, "NOTICES ON THE WALL (X: TAKE ONE)");                y += 12;
    for (int i = 0; i < missions_offer_count(); i++, row++) {
        missions_describe(missions_offer(i), t, d, sizeof t);
        gfx_button(LEFT, y, PANEL_W - 56, 15, TEXT_LAYER, lounge_sel == row ? BTN_SELECTED : BTN_NORMAL, 1, t);
        small(LEFT + 8, y + 16, 150, 200, 160, d);
        y += 27;
    }
    small(LEFT, y, 255, 200, 90, "THE TABLES (LEFT RIGHT: BET)");                       y += 12;
    for (int g = 0; g < GAME_COUNT; g++, row++) {
        gfx_button(LEFT, y, PANEL_W - 56, 15, TEXT_LAYER, lounge_sel == row ? BTN_SELECTED : BTN_NORMAL, 1,
                   lounge_game_line(g, lounge_bet[g]));
        y += 18;
    }
    hint("UP DOWN: CHOOSE   X: TAKE / PLAY   LEFT RIGHT: BET   O: BACK");
}

static void draw_guild(void) {
    char title[72], detail[72];
    if (guild_tab && !guild_syn) { draw_hires(); return; }
    int no = missions_offer_count(), na = missions_active_count();
    int y = TOP - 8;
    small(LEFT, y, 255, 200, 90, guild_syn ? "SYNDICATE JOBS (X: TAKE IT)" : "CONTRACTS ON OFFER (X: ACCEPT)   L1 R1: HIRE ESCORTS");
    y += 12;
    if (no == 0) { line1(y, 150, 170, 200, guild_syn ? "NOTHING FOR YOU TODAY." : "THE BOARD IS EMPTY TODAY."); y += 18; }
    for (int i = 0; i < no; i++) {
        missions_describe(missions_offer(i), title, detail, sizeof title);
        gfx_button(LEFT, y, PANEL_W - 56, 15, TEXT_LAYER, guild_sel == i ? BTN_SELECTED : BTN_NORMAL, 1, title);
        small(LEFT + 8, y + 16, 150, 200, 160, detail);
        y += 27;
    }
    y += 4;
    small(LEFT, y, 255, 200, 90, "YOUR CONTRACTS (SQUARE: ABANDON)");
    y += 12;
    if (na == 0) { line1(y, 150, 170, 200, "NONE YET. UP TO 4 AT ONCE."); y += 18; }
    for (int i = 0; i < na; i++) {
        missions_describe(missions_active(i), title, detail, sizeof title);
        gfx_button(LEFT, y, PANEL_W - 56, 15, TEXT_LAYER, guild_sel == no + i ? BTN_SELECTED : BTN_NORMAL, 1, title);
        small(LEFT + 8, y + 16, 150, 200, 230, detail);
        y += 27;
    }
    if (!guild_syn && missions_discoveries_unsold()) {
        snprintf(title, sizeof title, "SELL SURVEY DATA: %d DISCOVER%s", missions_discoveries_unsold(),
                 missions_discoveries_unsold() == 1 ? "Y" : "IES");
        gfx_button(LEFT, y + 4, PANEL_W - 56, 15, TEXT_LAYER, guild_sel == no + na ? BTN_SELECTED : BTN_NORMAL, 1, title);
    }
    hint("UP DOWN: CHOOSE   X: ACCEPT / SELL   SQUARE: ABANDON   O: BACK");
}

static void draw_black(void) {
    char line[72];
    static const char *what[BM_COUNT] = {
        "SMUGGLING RUNS AND JOBS NO HONEST STATION WILL POST",
        "PAY WHAT YOU OWE BEFORE THE COLLECTORS COME",
        "A HIDDEN HOLD: UFS SCANS MISS SOME OF YOUR CONTRABAND",
        "FENCE A HANGAR SHIP FOR 75% OF ITS PRICE, NOT 50%",
        "WIPE YOUR UFS AND FSA RECORDS BACK TO NEUTRAL",
        "WHERE THE VEYR HIVE IS. THE SYNDICATE WANTS WHAT'S IN IT TOO",
        "CONTAINMENT: CLAIM A DISABLED VEYR HULL WITHOUT IT BLOWING UP",
    };
    for (int r = 0; r < BM_COUNT; r++) {
        int y = TOP + r * 30;
        int ok = syn_level() >= bm_level[r];
        if (!ok) snprintf(line, sizeof line, "?????  (%s)", syn_level_name(bm_level[r]));
        else if (r == BM_JOBS) snprintf(line, sizeof line, "SYNDICATE JOBS");
        else if (r == BM_DEBT) {
            if (syn_debt > 0) snprintf(line, sizeof line, "PAY YOUR DEBT  %d  (DUE DAY %d)", syn_debt, syn_debt_due);
            else snprintf(line, sizeof line, "DEBTS: NONE");
        }
        else if (r == BM_HOLD) {
            if (syn_hold_level >= 3) snprintf(line, sizeof line, "HIDDEN HOLD %d UNITS  MAX", syn_hold_units());
            else snprintf(line, sizeof line, "HIDDEN HOLD %d UNITS  UPGRADE %d", syn_hold_units(), (int) hold_price[syn_hold_level]);
        } else if (r == BM_FENCE) {
            if (fleet_count == 0) snprintf(line, sizeof line, "FENCE: NO SHIPS IN THE HANGAR");
            else snprintf(line, sizeof line, "FENCE < %s >  %d", ship_looks[fleet[fence_sel]].name, fence_value(fleet[fence_sel]));
        } else if (r == BM_WIPE) snprintf(line, sizeof line, "WIPE RECORDS  %d", WIPE_PRICE);
        else if (r == BM_VEYR) {
            if (veyr_phase() == VP_INVADING && !veyr_revealed()) snprintf(line, sizeof line, "VEYR HIVE COORDINATES  %d", VEYR_COORDS_PRICE);
            else snprintf(line, sizeof line, veyr_revealed() ? "VEYR HIVE COORDINATES  BOUGHT" : "VEYR HIVE COORDINATES  ---");
        } else {
            int lv = veyr_contain();
            if (veyr_phase() == VP_DORMANT) snprintf(line, sizeof line, "VEYR CONTAINMENT RIG  ---");
            else if (lv >= CONTAIN_MAX) snprintf(line, sizeof line, "VEYR CONTAINMENT RIG %d  MAX", lv);
            else snprintf(line, sizeof line, "VEYR CONTAINMENT RIG %d  UPGRADE %d", lv, veyr_contain_price(lv + 1));
        }
        gfx_button(LEFT, y, PANEL_W - 56, 24, TEXT_LAYER, r == black_sel ? BTN_SELECTED : ok ? BTN_NORMAL : BTN_DISABLED, 2, line);
    }
    int y = TOP + BM_COUNT * 30 + 4;
    line1(y, 120, 220, 160, syn_level() >= bm_level[black_sel] ? what[black_sel] : "EARN MORE SYNDICATE STANDING TO SEE THIS");
    y += 26;
    snprintf(line, sizeof line, "SYNDICATE STANDING: %s (%d)", syn_level_name(syn_level()), syn_rep());
    line1(y, 255, 200, 90, line);                                                            y += 16;
    if (syn_level() < SYN_INNER_CIRCLE) {
        snprintf(line, sizeof line, "NEXT: %s AT %d", syn_level_name(syn_level() + 1), syn_level_needs(syn_level() + 1));
        line1(y, 150, 170, 200, line);                                                       y += 16;
    }
    line1(y, 150, 170, 200, "EARN IT: SELL CONTRABAND (DOUBLE IN UFS SPACE), SLIP UFS SCANS,");   y += 14;
    line1(y, 150, 170, 200, "SHOOT DOWN PATROLS, FENCE SHIPS. KILLING PIRATES COSTS YOU.");        y += 14;
    line1(y, 150, 170, 200, "TRUSTED OR BETTER: SYNDICATE CREWS LEAVE YOU ALONE.");
    hint("UP DOWN: CHOOSE   LEFT RIGHT: WHICH SHIP   X: BUY   O: BACK");
}

static void draw_trade(const Ship *s) {
    char line[72];
    int cap = s->def.cargo_cap;
    int w = PANEL_W - 56;

    snprintf(line, sizeof line, "HOLD %d/%d", cargo_total(), cap);
    line1(TOP - 2, 255, 200, 90, line);
    gfx_bar(LEFT + 120, TOP - 2, 160, 8, TEXT_LAYER, cargo_total(), cap, 255, 200, 90);

    gfx_text_big(LEFT + 10, TOP + 14, TEXT_LAYER, 1, 150, 170, 200,
                 "ITEM               BUY  SELL STOCK HOLD");

    for (int k = 0; k < TRADE_VISIBLE && trade_top + k < COMM_COUNT; k++) {
        int c = trade_top + k;
        int y = TOP + 28 + k * TRADE_ROW_PITCH;
        int role = market_role(where, c);
        snprintf(line, sizeof line, "%-16s%6d%6d%6d%5d  %s%s", commodities[c].name,
                 market_buy_price(where, c), market_sell_price(where, c),
                 market_stock(where, c), cargo[c], role > 0 ? "EXPORT" : role < 0 ? "IMPORT" : "",
                 commodities[c].contraband ? " !" : "");
        int st = c == trade_sel ? BTN_SELECTED : cargo[c] ? BTN_MARKED : BTN_NORMAL;
        gfx_button(LEFT, y, w, TRADE_ROW_H, TEXT_LAYER, st, 1, line);
    }
    /* Scroll marks, at the right of the column headings. */
    {
        char more[32] = "";
        int above = trade_top, below = COMM_COUNT - trade_top - TRADE_VISIBLE;
        if (above > 0 && below > 0) snprintf(more, sizeof more, "%d ABOVE, %d BELOW", above, below);
        else if (above > 0)         snprintf(more, sizeof more, "%d MORE ABOVE", above);
        else if (below > 0)         snprintf(more, sizeof more, "%d MORE BELOW", below);
        if (more[0]) gfx_text_big(LEFT + w - (int) strlen(more) * 8 - 6, TOP + 14, TEXT_LAYER, 1, 255, 200, 90, more);
    }

    /* Profit note, in the message line when no message is showing. */
    if (msg_timer == 0) {
        int y = PANEL_Y + PANEL_H - 50;
        if (commodities[trade_sel].contraband) {
            line1(y, 255, 140, 110, "! CONTRABAND: UFS SCANS SEIZE IT AND FINE YOU");
        } else if (cargo[trade_sel]) {
            int avg = (int) (cargo_cost[trade_sel] / cargo[trade_sel]);
            int per = market_sell_price(where, trade_sel) - avg;
            snprintf(line, sizeof line, "YOU PAID %d EACH ON AVERAGE. SELLING HERE NOW: %s%d EACH.", avg,
                     per >= 0 ? "PROFIT +" : "LOSS ", per);
            line1(y, 120, 220, 160, line);
        } else {
            line1(y, 150, 170, 200, "BUY WHERE IT IS CHEAP (EXPORT), SELL WHERE IT IS DEAR (IMPORT).");
        }
    }

    snprintf(line, sizeof line, "X BUY  SQ SELL  R1 TEN  TRI SELL ALL  SELECT: CHART %d  O BACK", CHART_PRICE);
    hint(line);
}

static int turntable_tab(void) {
    return yard_tab == TAB_REFIT || yard_tab == TAB_BUY || (yard_tab == TAB_FLEET && fleet_count > 0);
}

static void draw_shipyard(const Ship *s) {
    char line[64];

    /* Tabs. */
    int tx = LEFT;
    for (int t = 0; t < TAB_COUNT; t++) {
        int on = t == yard_tab;
        int bw = (int) strlen(tab_names[t]) * 16 + 20;
        gfx_button(tx, TOP, bw, 26, TEXT_LAYER, on ? BTN_SELECTED : BTN_NORMAL, 2, tab_names[t]);
        tx += bw + 8;
    }

    int rows = yard_rows();
    int y0 = TOP + 36;
    /* The SELL tab can grow long (stored modules): smaller rows, and it scrolls. */
    int pitch = yard_tab == TAB_SELL ? 18 : 26, bh = yard_tab == TAB_SELL ? 16 : 24, fs = yard_tab == TAB_SELL ? 1 : 2;
    if (yard_tab == TAB_BUY || yard_tab == TAB_FLEET) { pitch = 22; bh = 19; }   /* beside the turntable */
    int first = 0, shown = rows;
    if (yard_tab == TAB_SELL && rows > 12) {
        shown = 12;
        first = yard_sel - 6;
        if (first < 0) first = 0;
        if (first > rows - shown) first = rows - shown;
    }
    for (int row = first; row < first + shown; row++) {
        int y = y0 + (row - first) * pitch;
        int dim = 0;
        if (yard_tab == TAB_REFIT) {
            snprintf(line, sizeof line, "OPEN THE SHIP EDITOR");
        } else if (yard_tab == TAB_UPGRADES) {
            int price = mod_next_price(row);
            if (mod_illegal(row) && syn_level() < mod_needs_syn(row)) {
                snprintf(line, sizeof line, "?????  (%s)", syn_level_name(mod_needs_syn(row)));
                dim = 1;
            }
            else if (price) snprintf(line, sizeof line, "%s %d/%d  %d", mod_name(row),
                                mod_level[row], mod_max(row), price);
            else       snprintf(line, sizeof line, "%s MAX", mod_name(row));
        } else if (yard_tab == TAB_BUY) {
            int d = yard_ship_for_row(s, row);
            snprintf(line, sizeof line, "%s  %d", ship_looks[d].name, ship_cost(s, d));
        } else if (yard_tab == TAB_FLEET) {
            if (fleet_count == 0) { snprintf(line, sizeof line, "NO CLAIMED SHIPS"); dim = 1; }
            else if (!fleet_here(row)) {
                snprintf(line, sizeof line, "%s   AT %s", ship_looks[fleet[row]].name, galaxy_sys(fleet_sys[row])->name);
                dim = 1;
            }
            else snprintf(line, sizeof line, "%s  %s %s  %d", ship_looks[fleet[row]].name,
                          rank_short(fleet_rank(row)), (renaming && row == rename_fleet) ? name_buf : fleet_pname[row],
                          ship_trade_in_value(fleet[row]));
        } else if (row < mods_shown()) {
            if (mod_level[row]) snprintf(line, sizeof line, "UPGRADE: %s %d/%d  +%d", mod_name(row),
                                         mod_level[row], mod_max(row), mod_sell_value(row));
            else                { snprintf(line, sizeof line, "UPGRADE: %s NONE", mod_name(row)); dim = 1; }
        } else {
            int type, tier;
            store_row(row - mods_shown(), &type, &tier);
            snprintf(line, sizeof line, "STORED: %s T%d  x%d  +%d EACH", module_defs[type].name, tier,
                     store_count(type, tier), fit_tier_price(type, tier) * STORE_SELL_PCT / 100);
        }
        int bw = PANEL_W - 56;
        if (yard_tab == TAB_BUY || yard_tab == TAB_FLEET) { bw -= 196; fs = 1; }   /* room for the turntable */
        gfx_button(LEFT, y, bw, bh, TEXT_LAYER, row == yard_sel ? BTN_SELECTED : dim ? BTN_DISABLED : BTN_NORMAL, fs, line);
    }

    /* The turntable (M14d): the ship you're about to refit (or buy, or swap into), turning in 3D. */
    int card_bottom = 0;
    if (turntable_tab()) {
        int d = yard_tab == TAB_REFIT ? s->def_index : yard_tab == TAB_FLEET ? fleet[yard_sel]
              : yard_ship_for_row(s, yard_sel);
        int big = yard_tab == TAB_REFIT;
        int r = big ? 100 : 60;
        int cw = (r + 14) * 2, ch = r * 3 / 2 + 36;
        int cxl = big ? PANEL_X + (PANEL_W - cw) / 2 : PANEL_X + PANEL_W - 24 - cw;
        int cyt = big ? y0 + 34 : y0;
        if (d >= 0) {
            model_draw(ship_defs[d].sprite, cxl + cw / 2, cyt + ch / 2 - 6, r, (anim * 12) & 4095, 600, TEXT_LAYER);
            small(cxl + 8, cyt + ch - 12, 150, 170, 200, ship_looks[d].name);
            gfx_card(cxl, cyt, cw, ch, TEXT_LAYER);
            card_bottom = cyt + ch;
        }
    }

    /* What the highlighted row does. */
    int y = y0 + shown * pitch + 8;
    if (card_bottom && y < card_bottom + 8) y = card_bottom + 8;
    if (yard_tab == TAB_REFIT) {
        line1(y, 120, 220, 160, "PLACE WEAPONS, ENGINES, REACTORS, SHIELDS AND ARMOR ON YOUR GRID.");
        line1(y + 14, 150, 170, 200, "REMOVED MODULES GO TO STORAGE; REFITTING ONE COSTS A SMALL FEE.");
    } else if (yard_tab == TAB_UPGRADES) {
        line1(y, 120, 220, 160, mod_effect(yard_sel));
    } else if (yard_tab == TAB_BUY) {
        int d = yard_ship_for_row(s, yard_sel);
        const ShipDef *n = &ship_defs[d];
        snprintf(line, sizeof line, "%s: %s. YOUR SHIP TRADES IN FOR %d.", ship_looks[d].name,
                 class_names[n->cls], ship_trade_in_value(s->def_index));
        line1(y, 120, 220, 160, line);
        y += 14;
        snprintf(line, sizeof line, "HULL %d  SHIELD %d  SPEED %d PX/S  CARGO %d", n->hull_max, n->shield_arc_max,
                 speed_pxs(n), n->cargo_cap);
        line1(y, 120, 220, 160, line);
    } else if (yard_tab == TAB_FLEET) {
        if (renaming) {
            snprintf(line, sizeof line, "PILOT: %s_", name_buf);
            line1(y, 255, 230, 80, line);
            line1(y + 14, 150, 170, 200, "UP DOWN: LETTER  LEFT RIGHT: MOVE  X: SAVE  O: CANCEL");
        } else if (fleet_count > 0 && yard_sel < fleet_count) {
            snprintf(line, sizeof line, "PILOT %s, %s: %d KILLS", fleet_pname[yard_sel], rank_name(fleet_rank(yard_sel)), fleet_xp[yard_sel]);
            line1(y, 255, 205, 60, line);
            line1(y + 14, 120, 220, 160, "X: SWAP INTO IT  TRIANGLE: REFIT  SQUARE: SELL  SELECT: RENAME");
        }
    } else if (yard_sel < mods_shown()) {
        line1(y, 120, 220, 160, mod_effect(yard_sel));
        line1(y + 14, 150, 170, 200, "SELLING TAKES OFF THE TOP LEVEL FOR HALF ITS PRICE.");
    } else {
        line1(y, 120, 220, 160, "A MODULE IN STORAGE. SELLING IT PAYS 30% OF ITS PRICE.");
        line1(y + 14, 150, 170, 200, "OR KEEP IT: FITTING IT AGAIN COSTS ONLY A SMALL FEE.");
    }
    y += 34;
    snprintf(line, sizeof line, "YOURS NOW: HULL %d  SHIELD %d  SPEED %d PX/S  GUNS %d%%", s->def.hull_max,
             s->def.shield_arc_max, speed_pxs(&s->def), s->fire_pct);
    line1(y, 150, 170, 200, line);

    hint("LEFT RIGHT OR L1 R1: TAB   UP DOWN: CHOOSE   X: CONFIRM   O: BACK");
}

void dock_draw(const Ship *s) {
    if (!active) return;

    const NavObject *o = world_nav(where);
    char title[40];
    snprintf(title, sizeof title, "%s%s", o->name, o->kind == KIND_PLANET ? " SPACEPORT" : "");

    /* Text and pictures first: the panel goes in last, on a layer behind. */
    gfx_text_big(LEFT, PANEL_Y + 18, TEXT_LAYER, 3, 90, 220, 255, title);
    if (galaxy_sys(galaxy_current())->owner == OWN_PLAYER)          /* M14g: you took this system */
        gfx_text_big(LEFT, PANEL_Y + 52, TEXT_LAYER, 1, 255, 205, 60, "DOCKED. THIS PORT FLIES YOUR FLAG.");
    else
        gfx_text_big(LEFT, PANEL_Y + 52, TEXT_LAYER, 1, 120, 200, 140, "DOCKED. NO PIRATE CAN REACH YOU HERE.");
    char cr[24];
    snprintf(cr, sizeof cr, "CREDITS %d", credits);
    gfx_text_big(PANEL_X + PANEL_W - 28 - (int) strlen(cr) * 16, PANEL_Y + 48, TEXT_LAYER, 2, 255, 230, 80, cr);

    char when[24];
    snprintf(when, sizeof when, "DAY %d  %02d:%02d", clock_day(), clock_hour(), clock_minute());
    gfx_text_big(PANEL_X + PANEL_W - 28 - (int) strlen(when) * 8, PANEL_Y + 70, TEXT_LAYER, 1, 150, 170, 200, when);

    art_ship = ship_defs[s->def_index].sprite;
    if (page == PAGE_STATUS)        draw_status(s);
    else if (page == PAGE_SHIPYARD && editor_active()) editor_draw();
    else if (page == PAGE_SHIPYARD) draw_shipyard(s);
    else if (page == PAGE_TRADE)    draw_trade(s);
    else if (page == PAGE_BLACK)    draw_black();
    else if (page == PAGE_GUILD)    draw_guild();
    else if (page == PAGE_LOUNGE)   draw_lounge();
    else if (page == PAGE_COMMS)    draw_comms();
    else if (page == PAGE_SAVE)     draw_save_page();
    else                            draw_main();

    if (msg_timer > 0)
        line1(PANEL_Y + PANEL_H - 50, 255, 200, 90, msg);

    gfx_panel(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, PANEL_LAYER);
}
