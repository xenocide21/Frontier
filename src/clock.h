/*
 * FRONTIER - the game calendar.
 *
 * One game hour is 60 real seconds, so a day is 24 minutes of play. The clock
 * runs in flight and while docked. Markets change on the hour; the lounge
 * (Milestone 13) will seed what it offers from the day and hour.
 */
#ifndef FRONTIER_CLOCK_H
#define FRONTIER_CLOCK_H

#include "common.h"

#define FRAMES_PER_GAME_HOUR 3600

void clock_init(void);

/* Call once per frame. Returns 1 on the frame a new game hour begins. */
int  clock_tick(void);

/* Jumps the calendar forward by whole hours (the frame counter is untouched).
 * The caller runs market_hour() once per hour advanced. */
void clock_advance_hour(void);

int  clock_day(void);        /* 1, 2, 3 ... */
int  clock_hour(void);       /* 0..23 */
int  clock_minute(void);     /* 0..59 */
u8  *clock_ser(u8 *p, int load);

#endif
