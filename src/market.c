#include <string.h>

#include "clock.h"
#include "galaxy.h"
#include "market.h"
#include "mission.h"
#include "outfit.h"
#include "world.h"
#include "veyr.h"

const Commodity commodities[COMM_COUNT] = {
    { "FOOD",                  20, 300 },
    { "WATER",                 12, 300 },
    { "FUEL",                  40, 200 },
    { "METALS",                55, 200 },
    { "INDUSTRIAL PARTS",      90, 120 },
    { "ELECTRONICS",          140, 100 },
    { "MACHINERY",            120, 100 },
    { "MEDICAL SUPPLIES",     160,  80 },
    { "PHARMACEUTICALS",      300,  50 },
    { "WEAPONS",              350,  40 },
    { "AMMUNITION",           180,  80 },
    { "LUXURY GOODS",         500,  30 },
    { "SCIENTIFIC GEAR",      420,  30 },
    { "RAW MINERALS",          45, 250 },
    { "RARE MATERIALS",       800,  20 },
    { "SCRAP METAL",           60, 150 },   /* from salvaged ships */
    { "NARCOTICS",            380,  40, 1 },   /* contraband: the Syndicates sell it */
    { "STOLEN TECH",          520,  30, 1 },
};

u16 cargo[COMM_COUNT];
u32 cargo_cost[COMM_COUNT];

/* Commodity ids, for the economy table below. */
enum { FOOD, WATER, FUEL, METALS, PARTS, ELECTRONICS, MACHINERY, MEDICAL, PHARMA, WEAPONS,
       AMMO, LUXURY, SCIENCE, MINERALS, RARE, SCRAP };

#define MARKET_PLACES 2          /* per system: [0] the station, [1] the planet */
#define SPREAD_PCT 92            /* the player sells for 92% of the posted price */
#define PRICE_SWING 60           /* price moves by up to this % at 100% off target */
#define DRIFT_PCT 3              /* stock moves this % of the gap toward its natural level each hour */

static u16 stock[GALAXY_SYSTEMS][MARKET_PLACES][COMM_COUNT];

/* Exports are cheap (plentiful), imports dear (scarce): the GDD's economic types. */
static int role_for_econ(int econ, int c) {
    switch (econ) {
    case ECON_AGRICULTURAL:
        if (c == FOOD || c == WATER) return 1;
        if (c == MACHINERY || c == MEDICAL) return -1;
        break;
    case ECON_INDUSTRIAL:
        if (c == MACHINERY || c == PARTS || c == ELECTRONICS) return 1;
        if (c == METALS || c == MINERALS || c == FUEL || c == SCRAP) return -1;
        break;
    case ECON_MINING:
        if (c == METALS || c == MINERALS || c == RARE) return 1;
        if (c == FOOD || c == MACHINERY || c == FUEL) return -1;
        break;
    case ECON_MILITARY:
        if (c == WEAPONS || c == AMMO || c == FUEL || c == MEDICAL) return -1;
        break;
    case ECON_SETTLEMENT:
        if (c == MEDICAL || c == FUEL || c == MACHINERY || c == FOOD) return -1;
        break;
    case ECON_TRADE_HUB:
        if (c == LUXURY) return 1;
        break;
    }
    return 0;
}

/* Contraband: the Syndicates export it; everyone else's black market wants it. */
static int role_at(int sys, int place, int c) {
    if (commodities[c].contraband) return galaxy_founder(sys) == OWN_SYNDICATE ? 1 : -1;
    return role_for_econ(galaxy_sys(sys)->econ[place], c);
}

int market_role_at(int sys, int place, int c) { return role_at(sys, place, c); }
int market_role(int place, int c) { return role_at(galaxy_current(), place, c); }

/* The level stock settles toward today: exporters high, importers low, plus a
 * steady daily wobble of up to 15% so prices are not the same every day. */
static int natural_stock(int sys, int place, int c) {
    int role = role_at(sys, place, c);
    int mult = role > 0 ? 160 : role < 0 ? 40 : 100;
    if (commodities[c].contraband && galaxy_sys(sys)->owner == OWN_FEDERATION) return 0;   /* none on open sale */
    int noise = (int) (hash3(sys * MARKET_PLACES + place, c, clock_day()) % 31) - 15;
    return commodities[c].target * (mult + noise) / 100;
}

void market_init(void) {
    memset(cargo, 0, sizeof cargo);
    memset(cargo_cost, 0, sizeof cargo_cost);
    for (int s = 0; s < GALAXY_SYSTEMS; s++)
        for (int p = 0; p < MARKET_PLACES; p++)
            for (int c = 0; c < COMM_COUNT; c++)
                stock[s][p][c] = (u16) natural_stock(s, p, c);
}

void market_hour(void) {
    for (int s = 0; s < GALAXY_SYSTEMS; s++)
        for (int p = 0; p < MARKET_PLACES; p++)
            for (int c = 0; c < COMM_COUNT; c++) {
                int nat = natural_stock(s, p, c);
                int diff = nat - (int) stock[s][p][c];
                int step = diff * DRIFT_PCT / 100;
                if (step == 0 && diff != 0) step = diff > 0 ? 1 : -1;
                stock[s][p][c] = (u16) ((int) stock[s][p][c] + step);
            }
}

int market_stock(int place, int c) { return stock[galaxy_current()][place][c]; }

static int price_at(int sys, int place, int c) {
    int target = commodities[c].target;
    int r = ((target - (int) stock[sys][place][c]) * 100) / target;      /* + when scarce */
    if (r > 100) r = 100;
    if (r < -100) r = -100;
    int p = commodities[c].base * (100 + r * PRICE_SWING / 100) / 100;
    return p < 1 ? 1 : p;
}

/* A squadron's trade run (M15b): the best legal cargo from one system's station to
 * another's. Buys `units`, sells them there, moves both markets; returns the profit. */
int market_trade_run(int from, int to, int units, int *best_c) {
    int best = -1, margin = 0;
    for (int c = 0; c < COMM_COUNT; c++) {
        if (commodities[c].contraband) continue;
        int m = price_at(to, 0, c) * SPREAD_PCT / 100 - price_at(from, 0, c);
        if (m > margin) { margin = m; best = c; }
    }
    if (best_c) *best_c = best;
    if (best < 0 || units <= 0) return 0;
    int have = stock[from][0][best];
    if (units > have) units = have;
    stock[from][0][best] = (u16) (have - units);
    stock[to][0][best] = (u16) (stock[to][0][best] + units > 60000 ? 60000 : stock[to][0][best] + units);
    return units * margin;
}

static int posted_price(int place, int c) {
    int target = commodities[c].target;
    int r = ((target - (int) stock[galaxy_current()][place][c]) * 100) / target;      /* + when scarce */
    if (r > 100) r = 100;
    if (r < -100) r = -100;
    int p = commodities[c].base * (100 + r * PRICE_SWING / 100) / 100;
    return p < 1 ? 1 : p;
}

/* M18b: the hero of the war gets 10% better prices in their side's ports. */
static int hero_here(void) { return veyr_hero_port(galaxy_current()); }

int market_buy_price(int place, int c)  {
    int p = posted_price(place, c);
    if (hero_here()) p -= p / 10;
    return p < 1 ? 1 : p;
}

int market_sell_price(int place, int c) {
    int p = posted_price(place, c) * SPREAD_PCT / 100;
    if (hero_here()) p += p / 10;
    return p < 1 ? 1 : p;
}

u8 *market_ser(u8 *p, int load) {
    SER(p, load, cargo); SER(p, load, cargo_cost); SER(p, load, stock);
    return p;
}

int cargo_total(void) {
    int t = 0;
    for (int c = 0; c < COMM_COUNT; c++) t += cargo[c];
    return t + missions_cargo_units();          /* contract cargo and passengers take room too */
}

int cargo_contraband(void) {
    int t = 0;
    for (int c = 0; c < COMM_COUNT; c++) if (commodities[c].contraband) t += cargo[c];
    return t;
}

int cargo_seize_contraband(int keep, int *seized) {
    int v = 0;
    *seized = 0;
    for (int c = 0; c < COMM_COUNT; c++) {
        if (!commodities[c].contraband || cargo[c] == 0) continue;
        int k = keep < cargo[c] ? keep : cargo[c];      /* the smuggler's hold hides these */
        keep -= k;
        int n = cargo[c] - k;
        if (n == 0) continue;
        cargo_cost[c] = cargo_cost[c] * k / cargo[c];
        cargo[c] = (u16) k;
        v += n * commodities[c].base;
        *seized += n;
    }
    return v;
}

void cargo_clear(void) {
    memset(cargo, 0, sizeof cargo);
    memset(cargo_cost, 0, sizeof cargo_cost);
}

/* Units move one at a time so each one is priced at the stock it leaves behind. */
TradeResult market_buy(int place, int c, int qty, int hold_cap) {
    TradeResult r = { TRADE_OK, 0, 0, 0 };
    for (int i = 0; i < qty; i++) {
        if (stock[galaxy_current()][place][c] == 0)             { r.result = TRADE_NO_STOCK;   break; }
        if (cargo_total() >= hold_cap)        { r.result = TRADE_NO_ROOM;    break; }
        int price = market_buy_price(place, c);
        if (credits < price)                  { r.result = TRADE_NO_CREDITS; break; }
        credits -= price;
        stock[galaxy_current()][place][c]--;
        cargo[c]++;
        cargo_cost[c] += (u32) price;
        r.units++;
        r.credits += price;
    }
    if (r.units > 0) r.result = TRADE_OK;
    return r;
}

TradeResult market_sell(int place, int c, int qty) {
    TradeResult r = { TRADE_OK, 0, 0, 0 };
    int cost_removed = 0;
    for (int i = 0; i < qty; i++) {
        if (cargo[c] == 0) { r.result = TRADE_NO_CARGO; break; }
        int price = market_sell_price(place, c);
        int unit_cost = (int) (cargo_cost[c] / cargo[c]);
        credits += price;
        cargo_cost[c] -= (u32) unit_cost;
        cargo[c]--;
        if (stock[galaxy_current()][place][c] < 65535) stock[galaxy_current()][place][c]++;
        cost_removed += unit_cost;
        r.units++;
        r.credits += price;
    }
    r.profit = r.credits - cost_removed;
    if (r.units > 0) r.result = TRADE_OK;
    return r;
}
