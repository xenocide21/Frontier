/*
 * FRONTIER - commodities, markets and the player's cargo hold.
 *
 * Each place has a stock of every commodity. Price rises as stock falls below
 * the commodity's target and falls as it rises above it. Exporters hold
 * plenty (cheap), importers hold little (dear). Stock drifts toward the day's
 * natural level every game hour, and the player's own trades move it too.
 */
#ifndef FRONTIER_MARKET_H
#define FRONTIER_MARKET_H

#include "common.h"

#define COMM_SCRAP 15            /* id of SCRAP METAL, from salvaged ships */
#define COMM_MEDICAL 7
#define COMM_NARCOTICS 16        /* contraband: banned in UFS space (Milestone 13d) */
#define COMM_STOLEN_TECH 17
#define COMM_COUNT 18            /* prototype list; the final target is 20 */

typedef struct {
    const char *name;            /* upper case */
    u16 base;                    /* credits per unit at stock == target */
    u16 target;                  /* units a place normally holds */
    u8  contraband;              /* 1 = UFS patrols seize it (Syndicate systems sell it) */
} Commodity;

extern const Commodity commodities[COMM_COUNT];

/* The player's hold: units and total credits paid for them. */
extern u16 cargo[COMM_COUNT];
extern u32 cargo_cost[COMM_COUNT];

void market_init(void);

/* Call on the hour (see clock_tick). */
void market_hour(void);

/* place is a NAV_* index. */
int  market_stock(int place, int c);
int  market_buy_price(int place, int c);    /* what the player pays per unit now */
int  market_sell_price(int place, int c);   /* what the player gets per unit now */
int  market_role(int place, int c);         /* +1 exports (cheap), -1 imports (dear), 0 neither */

enum { TRADE_OK = 0, TRADE_NO_STOCK, TRADE_NO_ROOM, TRADE_NO_CREDITS, TRADE_NO_CARGO };

typedef struct {
    int result;                  /* TRADE_OK, or why nothing (more) moved */
    int units;                   /* units that moved */
    int credits;                 /* credits spent or earned in total */
    int profit;                  /* sells only: earned minus what those units cost */
} TradeResult;

/* hold_cap is the ship's cargo capacity. */
TradeResult market_buy(int place, int c, int qty, int hold_cap);
TradeResult market_sell(int place, int c, int qty);

int  cargo_total(void);
u8  *market_ser(u8 *p, int load);
int  market_role_at(int sys, int place, int c);   /* +1 export, -1 import, 0 neither */
int  market_trade_run(int from, int to, int units, int *best_c);   /* squadron trade (M15b): profit */
int  cargo_contraband(void);     /* units of contraband in the hold */
int  cargo_seize_contraband(int keep, int *seized);   /* takes all but `keep` units; returns their base value */
void cargo_clear(void);          /* the hold is lost (ship destroyed) */

#endif
