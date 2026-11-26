/*
 * FRONTIER - the ship editor (Milestone 13a): refit your ship at a shipyard.
 *
 * The hull's grid is drawn over its sprite. Pick a module with L1 / R1, move
 * the cursor with the D-pad and press Cross to install it (it costs its price).
 * Circle takes out the module under the cursor and refunds 30%. Square turns
 * the module you are placing (long weapons cannot turn). L2 / R2 swing the
 * firing arc of the weapon under the cursor (or of the one you are placing).
 * Select cycles the tier (1-3); a tier you have no blueprint for can be bought
 * with Cross. Start puts the hull's default fit back, with no refund. Triangle finishes,
 * once the ship has a bridge, an engine and enough power.
 */
#ifndef FRONTIER_EDITOR_H
#define FRONTIER_EDITOR_H

#include "ship.h"

/* Refit ship s, whose layout is l: your own ship (with &player_layout) or a
 * hangar ship (a scratch Ship with its fleet layout). */
void editor_open(Ship *s, Layout *l);
int  editor_active(void);
void editor_update(void);
void editor_draw(void);

#endif
