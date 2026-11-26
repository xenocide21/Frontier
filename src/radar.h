/*
 * FRONTIER - the radar (minimap), Milestone 12c.
 *
 * A square scope in the top-right corner, centred on your ship and turning
 * with the world (north is up). Range depends on the RADAR module:
 * level 0 sees 1200 px, each level adds 600 (up to 3000 px at level 3).
 *
 *   white square    the station          (always shown; on the rim if out of range)
 *   coloured disc   the planet           (always shown; on the rim if out of range)
 *   red             pirates              grey when disabled
 *   green           merchants
 *   blue / magenta  UFS / FSA patrols    blinking when they are hostile to you
 *   cyan            your escorts
 *   yellow corners  your combat target
 *
 * Ships out of range are not shown. Radar jammers (a black-market module) are
 * planned and will hide a ship until it is close.
 */
#ifndef FRONTIER_RADAR_H
#define FRONTIER_RADAR_H

#include "ship.h"

#define RADAR_BASE_RANGE  1200
#define RADAR_STEP_RANGE   600

int  radar_range(void);
void radar_draw(const Ship *player, int target_slot, int frame);

#endif
