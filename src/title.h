/*
 * FRONTIER - the title screen, pilot select and opening briefing (Milestone 14a).
 */
#ifndef FRONTIER_TITLE_H
#define FRONTIER_TITLE_H

#include "common.h"

enum { PILOT_CHAD = 0, PILOT_REGINALD, PILOT_ZSAZSA, PILOT_MARIGOLD, PILOT_COUNT };
#define TITLE_LOAD (-1)

typedef struct {
    const char *name;          /* as the game shows it (fits PILOT_NAME_MAX) */
    const char *full_name;
    u8  ship, start_sys;
    s8  ufs, fsa;
    s32 credits;
    s32 debt;                  /* to a Syndicate lender, due on day 30 */
    const char *tag;           /* one-line pitch */
    const char *intro[9];      /* the opening narration, pre-wrapped; NULL ends it */
} Pilot;

extern const Pilot pilots[PILOT_COUNT];

/* Runs the title screen until the player starts a new game (returns the
 * pilot, after the difficulty and the briefing; game_difficulty is set) or
 * picks a save to load (TITLE_LOAD; the slot is in title_load_slot). notice,
 * if set, is shown once (e.g. why a load failed). open_load starts on the
 * load screen (LOAD GAME from the in-flight menu). */
int title_run(const char *notice, int open_load);
extern int title_load_slot;

#endif
