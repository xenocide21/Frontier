#include "clock.h"

static int day, hour, frames;

void clock_init(void) {
    day = 1;
    hour = 8;
    frames = 0;
}

int clock_tick(void) {
    if (++frames < FRAMES_PER_GAME_HOUR) return 0;
    frames = 0;
    if (++hour >= 24) { hour = 0; day++; }
    return 1;
}

void clock_advance_hour(void) {
    if (++hour >= 24) { hour = 0; day++; }
}

u8 *clock_ser(u8 *p, int load) { SER(p, load, day); SER(p, load, hour); SER(p, load, frames); return p; }

int clock_day(void)    { return day; }
int clock_hour(void)   { return hour; }
int clock_minute(void) { return frames * 60 / FRAMES_PER_GAME_HOUR; }
