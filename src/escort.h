/*
 * FRONTIER - escorts: your claimed ships flying with you (Milestone 12b).
 *
 * The first MAX_ESCORTS claimed ships whose stance is not HANGAR fly alongside
 * whatever ship you pilot. Stances (set in the Start menu's FLEET window):
 *   HANGAR  stays behind (does not fly)
 *   FOLLOW  keeps formation on you and never starts a fight
 *   GUARD   follows, and attacks whoever is shooting at you (not your target)
 *   ATTACK  goes after whatever you are attacking (your selected target)
 * Escorts that are destroyed are lost from your fleet. Enemy AI does not hunt
 * them yet; they take stray shots and hits meant for you.
 */
#ifndef FRONTIER_ESCORT_H
#define FRONTIER_ESCORT_H

#include "common.h"
#include "ship.h"

#define MAX_WING        3        /* slots 0..2: your wing, flying with you */
#define MAX_ESCORTS     6        /* slots 3..5: a squadron garrisoning this system (M15a) */
#define ESCORT_ID_BASE  200      /* collision ids 200..205 (the player is 255) */

extern int escort_threat;   /* enemy slot that recently shot the player, or -1 */
void escort_init(void);
/* target_slot: the player's selected combat target (an enemy slot) or -1. */
void escort_update(const Ship *player, int target_slot);
void escort_place_near(const Ship *player);       /* after a jump */
void escort_add_hittables(void);
void escort_damage(int id, int damage, int hit_x, int hit_y);
void escort_draw(s32 cam_x, s32 cam_y, int frame);
int  escort_count(void);
const Ship *escort_ship(int k);
void escort_refit_all(void);     /* after a hangar ship's fit changed */                  /* k < MAX_ESCORTS; NULL if empty or destroyed */
void escort_scored(int k);                        /* escort k downed a ship: its pilot learns (M15b) */
const char *escort_take_message(void);            /* a promotion, once; or 0 */
int  escort_take_lost(void);                      /* ships lost since the last call */

#endif
