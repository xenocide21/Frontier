/*
 * FRONTIER - small deterministic random number generator (xorshift32).
 *
 * Same seed, same sequence, so a run can be reproduced when hunting bugs.
 */
#ifndef FRONTIER_RNG_H
#define FRONTIER_RNG_H

#include "common.h"

extern u32 rng_state;

static inline u32 rng_next(void) {
    u32 x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}

/* Random integer in 0 .. n-1 (n must be above zero). */
static inline int rng_range(int n) {
    return (int) ((rng_next() >> 8) % (u32) n);
}

#endif
