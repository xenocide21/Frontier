/*
 * FRONTIER - the Start menu (system menu), opened in flight. The game pauses.
 *   RESUME, FLEET (stances for your claimed ships), GALAXY MAP, OPTIONS
 *   (flight assist and the test aids), CONTROLS (what each button does),
 *   SAVE / LOAD / QUIT (not available yet: saving comes in Milestone 14).
 */
#ifndef FRONTIER_SYSMENU_H
#define FRONTIER_SYSMENU_H

#include "common.h"

enum { SM_NONE = 0, SM_MAP, SM_ASSIST, SM_STRESS, SM_PEACE, SM_SWITCH, SM_LOAD, SM_QUIT,
       /* Dev menu (needs the cheat code; sysmenu_dev_arg carries the value) */
       SM_DEV_GOD, SM_DEV_GANG, SM_DEV_PIRATE, SM_DEV_WRECK, SM_DEV_SHIP, SM_DEV_WARP, SM_DEV_CREDITS,
       SM_DEV_REFILL, SM_DEV_UFS, SM_DEV_FSA, SM_DEV_SYN, SM_DEV_CONTRA, SM_DEV_MISSION,
       SM_ATTACK, SM_DEV_CAPTURE, SM_DEV_ADDSHIP, SM_DEV_HOURS, SM_DEV_VEYR };
extern int sysmenu_dev_arg;
/* The dev menu is on every disc, but hidden (M16b): the title screen's cheat code
 * turns it on for the game you then start or load. Never saved. */
extern int cheats_on;
/* ATTACK COLONY (M14g): set before sysmenu_open. block: 0 if allowed, else why not. */
extern const char *sysmenu_attack_block;
extern char sysmenu_attack_what[32], sysmenu_attack_owner[24];
void sysmenu_say(const char *s);

void sysmenu_open(void);
int  sysmenu_active(void);
int  sysmenu_update(void);                                  /* returns an SM_* request */
void sysmenu_draw(int assist, int stress, int peace, int god);

#endif
