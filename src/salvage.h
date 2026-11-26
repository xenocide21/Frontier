/*
 * FRONTIER - boarding and salvaging a disabled ship (Milestone 10).
 *
 * Fly slowly next to a disabled ship, with no active pirate nearby, and press
 * Start to board. A full-screen menu offers:
 *   CLAIM SHIP   add it to your fleet (kept in outfit.c: fleet[], fleet_count)
 *   SALVAGE      strip the hull into SCRAP METAL cargo (sold at a trading post)
 *   STRIP        take its spare cargo and credits (once per wreck)
 *   LEAVE
 * Every action has a chance to set off the derelict's self-destruct: you flee to
 * your ship and the wreck is lost. Strip and leave can be repeated.
 * The game is paused while the menu is open. Claimed ships go to the hangar: swap into or sell them in a shipyard's FLEET tab (Milestone 11).
 */
#ifndef FRONTIER_SALVAGE_H
#define FRONTIER_SALVAGE_H

#include "common.h"
#include "ship.h"

#define SALVAGE_RANGE     110      /* pixels from the wreck to board it */

enum { SALV_NONE = 0, SALV_REMOVE, SALV_LOOTED, SALV_CLOSE, SALV_DESTRUCT, SALV_BLAST };   /* BLAST: a Veyr wreck went up with you near it (M17b) */

/* looted: the wreck has already been stripped. */
void salvage_open(int def_index, int hull_pct, int looted);
int  salvage_active(void);

/* Handles the buttons. Returns SALV_REMOVE (the wreck is gone: claimed or sold),
 * SALV_LOOTED (it was stripped), SALV_CLOSE (left it alone), SALV_DESTRUCT (it blew up) or SALV_NONE. */
int  salvage_update(const Ship *player);
void salvage_draw(const Ship *player);

#endif
