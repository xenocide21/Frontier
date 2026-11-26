/*
 * FRONTIER - planet conquest (Milestone 14g).
 *
 * Fly near a colony's defence base (a station or the planet) and choose
 * ATTACK COLONY on the Start menu. The owner turns hostile at once (the UFS
 * and FSA to -60, the Syndicate to Marked; the independents only defend).
 * The base launches waves of defenders, one after another: more waves for a
 * better-defended system, and the last one is led by a flagship. No troops:
 * when the last defender is destroyed or disabled, the system is yours for
 * good. Humans never take it back (the Veyr can, later). Each game day every
 * system you own pays you tribute.
 *
 * Jumping away, being destroyed or reloading calls the assault off; the owner
 * stays hostile.
 */
#ifndef FRONTIER_CONQUEST_H
#define FRONTIER_CONQUEST_H

#include "common.h"
#include "ship.h"

#define ASSAULT_RANGE 1200          /* pixels from the base to start an assault */

void conquest_init(void);
const char *conquest_why_not(const Ship *pl);   /* 0 if an assault can start here, else the reason */
const char *conquest_target_name(void);         /* the base here, e.g. "HOME STATION" */
const char *conquest_owner_name(void);          /* who holds this system */
const char *conquest_start(const Ship *pl);     /* begins it; returns the message */
const char *conquest_update(const Ship *pl);    /* once a frame in flight: a message or 0 */
void conquest_abort_quiet(void);
int  conquest_active(void);
int  conquest_hud(char *buf, int n);            /* the assault line for the HUD; 0 if none */
const char *conquest_capture(int sys);          /* the system becomes yours (also the dev menu) */
int  conquest_tribute_of(int sys);              /* credits a day */
const char *conquest_tribute(void);             /* once a frame: pays on a new day, returns a message */
void conquest_declare(int sys);                 /* the owner turns hostile (an assault, or a squadron's) */
int  conquest_defence_strength(int sys);        /* all its waves, as one number (M15b) */
/* The war after the war (M18b). The UFS's admiral brings every FSA world to heel; the FSA's
 * marshal frees every UFS world; the Rim Crown takes them all. A world taken for a navy
 * flies its flag (and pays you a war prize); one taken for the crown is yours. */
int  conquest_war_left(void);                   /* enemy worlds still standing, or -1 if no war */
int  conquest_war_won(void);
const char *conquest_war_status(void);          /* "4 FSA WORLDS LEFT" etc., or 0 */
u8  *conquest_ser(u8 *p, int load);

#endif
