/*
 * FRONTIER - missions, the mercenary guild and discoveries (Milestone 13j).
 *
 * Guild stations post a board of contracts from whoever runs the system (UFS,
 * FSA or independents); the black market in Syndicate systems posts its own.
 * Up to MISSION_ACTIVE contracts at once. Deliveries finish when you dock at
 * the destination; bounties, interceptions and surveys finish in flight and
 * pay at once. Missed deadlines fail and cost a little standing.
 *
 * Every system also hides one anomaly. Fly within range of it and it's a
 * discovery: logged, and the survey data sells at any guild.
 */
#ifndef FRONTIER_MISSION_H
#define FRONTIER_MISSION_H

#include "common.h"
#include "ship.h"

enum { MI_NONE = 0, MI_CARGO, MI_COURIER, MI_SUPPLY, MI_PASSENGER, MI_BOUNTY, MI_INTERCEPT,
       MI_SMUGGLE, MI_SURVEY,
       MI_CAPTURE, MI_CLAIM,      /* story only (M18a): take a system; claim a Veyr hull */
       MI_TYPES };
enum { GIVER_UFS = 0, GIVER_FSA, GIVER_IND, GIVER_SYN };

#define MISSION_OFFERS 5
#define MISSION_ACTIVE 4

typedef struct {
    u8  type, giver;
    u8  dest_sys, dest_nav;   /* where to deliver, or which system to hunt / survey in */
    u8  commodity, units;     /* cargo carried, passengers, or pirates to destroy */
    u8  progress;             /* interception: pirates destroyed so far */
    u8  target_def, name;     /* bounty: the wanted pilot's ship and name */
    u8  spawned;              /* bounty: the target is out there now */
    u8  story;                /* 0, or 1 + the story track this is a step of (M18a) */
    s32 reward;
    s32 deadline;             /* absolute game hour, 0 = none */
} Mission;

void missions_init(void);

/* The board (rebuilt when you dock). syndicate = the black market's board. */
enum { BOARD_GUILD = 0, BOARD_SYNDICATE = 1, BOARD_LOUNGE = 2 };
void missions_board_make(int sys, int nav, int board);   /* BOARD_* */
void missions_make_custom(Mission *m, int type, int sys, int nav, int pct);
const char *missions_accept_custom(const Mission *m, int cargo_cap);
const char *missions_wanted_story(int i);                /* story bounty names (M18a) */
int  missions_story_active(int track);                   /* that track's step is on your list */
const char *missions_wanted_name(int i);
int  missions_discovered(int sys);
int  missions_offer_count(void);
const Mission *missions_offer(int i);
const char *missions_accept(int i, int cargo_cap);   /* a message for the player */

int  missions_active_count(void);
const Mission *missions_active(int i);
void missions_abandon(int i);

/* Two lines describing a contract (title, details), for the board. */
void missions_describe(const Mission *m, char *title, char *detail, int len);

/* Hold space the contracts use (cargo, passengers, contraband) and the contraband in it. */
int  missions_cargo_units(void);
int  missions_contraband(void);
void missions_seize_contraband(void);
void missions_ship_lost(void);             /* carrying contracts fail with the ship */      /* a UFS scan found it: smuggling contracts fail */

/* Call on docking; finishes deliveries here. Returns a message, or 0. */
const char *missions_on_dock(int sys, int nav);
/* Call once a flight frame. Returns a message, or 0. */
const char *missions_update(const Ship *player, int sys);
/* One HUD line about the nearest live objective in this system, or 0. */
const char *missions_hud(const Ship *player, int sys);
/* The wanted pilot's name for the target readout. */
const char *missions_bounty_name(void);

/* Discoveries (one hidden anomaly per system). */
int  missions_discoveries_unsold(void);
u8  *missions_ser(u8 *p, int load);
int  missions_sell_data(void);             /* returns credits paid */
void missions_draw_anomaly(s32 cam_x, s32 cam_y, int sys, int frame);

#ifdef FRONTIER_DEV
void missions_dev_finish(void);            /* dev menu: completes every active contract */
#endif

#endif
