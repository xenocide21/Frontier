/*
 * FRONTIER - docking and the station menus.
 *
 * The station and the planet can both be docked with. To dock the ship must be
 * inside the docking range (station: 90 px; planet: over the planet's disc) and
 * slower than DOCK_MAX_SPEED, then Start is pressed. Too fast and the dock
 * refuses. A docked ship is safe from pirates. The menu offers ship status and
 * repair now; trading (Milestone 8) and the shipyard (Milestone 7) are shown
 * but locked.
 */
#ifndef FRONTIER_DOCK_H
#define FRONTIER_DOCK_H

#include "common.h"
#include "ship.h"

#define DOCK_MAX_SPEED 6144   /* FP12 pixels per frame: 1.5 px/frame = 90 px/s */

enum { DOCK_NOTHING = 0, DOCK_TOO_FAST, DOCK_READY };
enum { DOCK_EVENT_NONE = 0, DOCK_EVENT_LAUNCH };

void dock_init(void);

/* Is anything dockable in range? Returns DOCK_NOTHING, DOCK_TOO_FAST or
 * DOCK_READY, and for the last two the NAV_* index of the object. */
int  dock_check(const Ship *s, int *nav_index);

void dock_enter(Ship *s, int nav_index);   /* the ship is now docked */
int  dock_active(void);

/* Handles the menu buttons while docked. Returns DOCK_EVENT_LAUNCH when the
 * pilot chooses LAUNCH; the ship is then undocked. */
int  dock_update(Ship *s);

void dock_draw(const Ship *s);

void dock_autosave(const Ship *s, int nav_index);   /* M14f: save to the autosave slot */

#endif
