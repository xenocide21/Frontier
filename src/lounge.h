/*
 * FRONTIER - the lounge and escort hiring (Milestone 14b).
 *
 * Every dock has a lounge. What's there depends on the place, the day and the
 * time (a seed of the three, so a revisit at the same time shows the same):
 *   rumours: trade tips (most are good, some stale), danger, and talk about you;
 *   an encounter: a stranger with a passenger request or a quest hook, or nobody;
 *   a small job board; and gambling at fixed, posted odds.
 * Guilds (stations with a shipyard) also hire out escort pilots by the day.
 */
#ifndef FRONTIER_LOUNGE_H
#define FRONTIER_LOUNGE_H

#include "common.h"

#define LOUNGE_RUMOURS 3
#define HIRE_OFFERS    3

void lounge_make(int sys, int nav);              /* on entering the lounge */
const char *lounge_rumour(int i);                /* two lines joined by '|', or 0 */
const char *lounge_encounter(void);              /* who's at the bar */
int  lounge_has_offer(void);                     /* the stranger offers a contract */
const char *lounge_offer_line(void);             /* what it pays */
const char *lounge_take_offer(int cargo_cap);

enum { GAME_HIGH_CARD = 0, GAME_SEVENS, GAME_COUNT };
extern const s32 lounge_bets[4];
const char *lounge_game_line(int game, int bet_index);
const char *lounge_gamble(int game, int bet_index);

/* Escort hiring at the guild. */
void hire_make(int sys, int nav);
int  hire_count(void);
const char *hire_line(int i);
const char *hire_take(int i);

#endif
