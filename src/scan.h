/*
 * FRONTIER - patrol scans and smuggling (Milestone 13d).
 *
 * UFS patrols hail the player and scan his cargo. Hold still close to them
 * for a few seconds and the scan runs; contraband (NARCOTICS, STOLEN TECH,
 * sold cheap in Syndicate space and dear everywhere else) is seized, with a
 * fine of half its base value and a small loss of UFS standing. Run (fly off,
 * keep moving, or jump) and that system's patrols open fire. Escape the
 * system and nothing follows you: no reputation is lost for running.
 * A Veyr ship in your fleet is never tolerated: UFS patrols attack on sight.
 * FSA patrols only circle and look you over, then move on.
 */
#ifndef FRONTIER_SCAN_H
#define FRONTIER_SCAN_H

#include "ship.h"

void scan_reset(void);           /* on arriving in a system */
/* Call once a flight frame. Returns a message to show, or 0. */
const char *scan_update(const Ship *player, int jumping, int docked);
int  scan_active(void);          /* a UFS patrol is waiting on you */
int  scan_progress_pct(void);    /* -1 unless a scan is running */

#endif
