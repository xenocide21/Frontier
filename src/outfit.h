/*
 * FRONTIER - the player's money, name and fitted modules.
 *
 * Modules belong to the pilot, not the hull: they carry over when a ship is
 * traded in. Each module has levels 0..MOD_MAX_LEVEL and changes the numbers of
 * the ship's definition (outfit_compute), so every system that reads ship->def
 * feels the change without further code.
 */
#ifndef FRONTIER_OUTFIT_H
#define FRONTIER_OUTFIT_H

#include "common.h"
#include "ship.h"

/* Hull upgrades (the shipyard's UPGRADES tab). They belong to the pilot and carry
 * over to any ship. Mass, Storage and Maneuvering go to level 10, each level
 * costing 2.2 times the last; Radar goes to level 3. */
enum { MOD_MASS = 0, MOD_STORAGE, MOD_MANEUVER, MOD_RADAR,
       MOD_SCOOP,                     /* antimatter scoop (M15a): refuels in dead space */
       MOD_JAMMER, MOD_TRANSPONDER,   /* illegal tech: Syndicate shipyards only (M14b) */
       MOD_BIOREGEN, MOD_NERVELINK,   /* augmented Veyr tech: only at a seized hive (M17b) */
       MOD_COUNT };
int mod_alien(int mod);               /* 1 = Veyr tech */
int mod_illegal(int mod);             /* 1 = only a Syndicate yard sells it */
int mod_needs_syn(int mod);           /* Syndicate standing level needed to buy it */
#define MOD_MAX_LEVEL 10
int mod_max(int mod);
#define MASS_PCT     20      /* + mass allowance per level */
#define STORAGE_PCT  20      /* + cargo capacity per level */
#define MANEUVER_PCT  8      /* + turn rate per level */
int outfit_mass_pct(void);   /* 100 + MASS_PCT * level */

#define PILOT_NAME_MAX 24
#define START_CREDITS  10000   /* placeholder: generous so every shop can be tested */

extern int  credits;
extern char pilot_name[PILOT_NAME_MAX];
extern u8   mod_level[MOD_COUNT];
extern int  fuel;                  /* hyperspace fuel units */
#define FLEET_MAX 8
extern u8   fleet[FLEET_MAX];     /* ship def indices of claimed ships */
extern u8   fleet_stance[FLEET_MAX];   /* STANCE_* for each claimed ship */
extern int  fleet_count;
enum { STANCE_HANGAR = 0, STANCE_FOLLOW, STANCE_GUARD, STANCE_ATTACK, STANCE_COUNT };
const char *stance_name(int stance);
void fleet_add(int def_index);          /* claims a ship (follows you by default) */
void fleet_remove(int i);               /* sold, or lost in battle */
/* Hired escorts (M14b): the game day their contract ends; 0 = a ship you own. */
extern u16 fleet_hired[FLEET_MAX];
const char *fleet_contracts_check(void); /* once a game hour: hired ships whose time is up fly home */
extern int  fuel_max;              /* the current ship's tank */

/* Fleets (M15a). Every claimed ship is somewhere: fleet_sys[i]. It belongs to your
 * WING (fleet_sq 0: flies with you, or waits docked if its stance is HANGAR) or to
 * squadron A, B or C (1..3), which holds a system or travels on its own orders.
 * Ships flying with you jump free: only the ship you pilot burns fuel. A squadron
 * travelling alone burns its lead ship's fuel, buys more at any port (credits), and
 * is STRANDED where it can't: put its ships in your wing to fly them out. */
#define SQUADRONS 4                       /* 0 = your wing, 1..3 = A, B, C */
enum { SQ_HOLD = 0, SQ_MOVE, SQ_STRANDED };
/* What a squadron does when it gets there (M15b): stay, run cargo back and forth, or take the colony. */
enum { MISSION_GO = 0, MISSION_TRADE, MISSION_ATTACK, MISSION_COUNT };
extern u8  sq_mission[SQUADRONS], sq_home[SQUADRONS];
const char *mission_name(int m);
const char *sq_order_ship(int q, int dest, int mission);   /* like sq_send, with a mission */

/* Pilots (M15b): every fleet ship has a named pilot who learns with each kill. */
#define FLEET_PILOT_LEN 12
extern char fleet_pname[FLEET_MAX][FLEET_PILOT_LEN];
extern u16  fleet_xp[FLEET_MAX];            /* ships destroyed or disabled */
enum { RANK_ROOKIE = 0, RANK_PILOT, RANK_VETERAN, RANK_ACE, RANK_COUNT };
int  fleet_rank(int i);
const char *rank_name(int r);               /* ROOKIE, PILOT, VETERAN, ACE */
const char *rank_short(int r);              /* RKE, PLT, VET, ACE */
const char *fleet_scored(int i);            /* a kill: returns a message on a promotion, else 0 */
int  fleet_strength(int i);                 /* rough fighting worth of the ship and its pilot */
int  def_strength(int def);                 /* the same for an unflown hull */
extern u8  fleet_sys[FLEET_MAX], fleet_sq[FLEET_MAX], fleet_fuel[FLEET_MAX];
extern u8  sq_order[SQUADRONS], sq_dest[SQUADRONS], sq_next[SQUADRONS];
extern u16 sq_eta[SQUADRONS];             /* game hours to the next system */
const char *sq_name(int q);               /* "WING", "SQUADRON A" ... */
int  sq_count(int q);                     /* ships in it */
int  sq_lead(int q);                      /* its first ship, or -1 */
int  sq_where(int q);                     /* the system it is in, or -1 if empty */
const char *sq_send(int q, int dest);     /* orders it to a system; returns a message */
const char *fleet_hour(void);             /* each game hour: travel, refuelling; a message or 0 */
void fleet_jumped(int from, int to);      /* you jumped: your wing goes with you */
int  fleet_here(int i);                   /* the ship is in the system you are in */
u8  *fleet_ser(u8 *p, int load);

/* Antimatter scoop (M15a): out in dead space (clear of planets and stations) it
 * gathers fuel, one unit every SCOOP_FRAMES / level frames. */
#define SCOOP_FRAMES 1200
int  scoop_update(int in_dead_space);     /* once a frame in flight; 1 while scooping */

#define BOOST_FUEL_FRAMES 180       /* afterburner: one fuel unit per 3 seconds (M16a) */
#define FUEL_PRICE 5               /* credits per unit at a station */

/* Call after the ship changes: sets the tank size and trims the fuel to it. */
void outfit_sync_fuel(const Ship *s);

void outfit_init(void);
void outfit_default_name(void);           /* back to Chad Beefsteel */

const char *mod_name(int mod);
const char *mod_effect(int mod);          /* one line: what a level does */
int         mod_next_price(int mod);      /* 0 when already at the top level */

/* Buys the next level if affordable. Returns 1 on success. */
int  mod_buy(int mod);

/* Sells the top level of a module back for half its price. Returns the credits
 * paid, or 0 if the module is not fitted. mod_sell_value gives the same sum
 * without selling. */
int  mod_sell_value(int mod);
int  mod_sell(int mod);

/* Value of a ship when traded in. */
int  ship_trade_in_value(int def_index);

/* Table entry for def_index with the fitted modules applied. */
void outfit_compute(int def_index, ShipDef *out, u8 *fire_pct, FitResult *fit, const u8 *hp);

/* The player's fitted modules (the ship editor changes this). Reset to the hull's
 * default fit whenever the player changes ship. */
extern Layout player_layout;
extern int    player_layout_def;

/* Each fleet ship's own fit (fleet_layout[i] goes with fleet[i]). */
extern Layout fleet_layout[];

/* Blueprints: the highest tier (1-3) of each module type the pilot may install. */
extern u8 blueprint[MT_COUNT];
u8 *outfit_ser(u8 *p, int load);

/* Module storage (M14b): modules taken off a ship are kept, not sold. Fitting
 * one from storage costs a small fee; selling one (shipyard SELL tab) pays 30%. */
#define FIT_FEE_PCT  10
#define FIT_FEE_MIN  25
#define STORE_SELL_PCT 30
int  store_count(int type, int tier);
void store_add(int type, int tier);
int  store_take(int type, int tier);       /* 1 if one was there */
int  store_rows(void);                     /* kinds of module in storage */
void store_row(int i, int *type, int *tier);
int  store_sell(int type, int tier);       /* credits paid, 0 if none */
int  fit_fee(int type, int tier);
void store_backup(int restore);            /* 0 = remember, 1 = put back (undo a reset) */

/* After a module purchase or sale: recomputes the ship's stats, tops up whatever
 * an upgrade raised so the change shows at once, and trims what a sale lowered. */
void outfit_refit(Ship *s);

#endif
