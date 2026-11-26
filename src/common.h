/*
 * FRONTIER - shared types and fixed-point helpers.
 *
 * The PlayStation has no floating-point unit, so all game maths uses integers.
 *
 *   FP12   signed 32-bit number with 12 fractional bits (4096 = 1.0).
 *          World positions and velocities use it. Velocity is "pixels per frame".
 *   Angle  0..4095 is one full turn. 0 points up the screen, 1024 points right.
 *          This matches the SDK's isin()/icos(), which return FP12 results.
 */
#ifndef FRONTIER_COMMON_H
#define FRONTIER_COMMON_H

#include <stdint.h>
#include <string.h>

typedef uint8_t  u8;
typedef int8_t   s8;
typedef uint16_t u16;
typedef int16_t  s16;
typedef uint32_t u32;
typedef int32_t  s32;

#define FP_SHIFT   12
#define FP_ONE     (1 << FP_SHIFT)
#define TO_FP(n)   ((s32)(n) << FP_SHIFT)
#define FROM_FP(n) ((s32)(n) >> FP_SHIFT)   /* arithmetic shift: rounds down */

#define ANGLE_FULL 4096
#define ANGLE_MASK 4095

/* 640x480 interlaced (NTSC). One world pixel is one screen pixel, so the
 * visible area is four times larger than the old 320x240 mode. */
#define SCREEN_W   640
#define SCREEN_H   480

/* Frames per second used to turn "per frame" values into "per second". */
#define FRAMES_PER_SECOND 60   /* NTSC. PAL (50) is a later task. */

static inline s32 abs32(s32 v) { return v < 0 ? -v : v; }

/* Cheap length of a 2D vector (error about 4%). Avoids a square root. */
static inline s32 approx_len(s32 x, s32 y) {
    s32 ax = abs32(x), ay = abs32(y);
    s32 hi = ax > ay ? ax : ay;
    s32 lo = ax > ay ? ay : ax;
    return (hi * 61 + lo * 26) >> 6;   /* 0.953*hi + 0.406*lo */
}

/* Integer square root (rounds down). */
static inline u32 isqrt32(u32 n) {
    u32 r = 0, bit = 1u << 30;
    while (bit > n) bit >>= 2;
    while (bit) {
        if (n >= r + bit) { n -= r + bit; r = (r >> 1) + bit; }
        else              { r >>= 1; }
        bit >>= 2;
    }
    return r;
}

/* Small deterministic hash used for stars and beacons: same input, same output. */
static inline u32 hash3(s32 a, s32 b, s32 c) {
    u32 h = (u32)a * 0x9E3779B1u ^ (u32)b * 0x85EBCA77u ^ (u32)c * 0xC2B2AE3Du;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return h;
}

/* Save games (M14a): each module copies its state to or from a buffer.
 * SER moves one variable and advances the pointer. */
#define SER(p, load, var) do { \
        if (load) memcpy(&(var), (p), sizeof(var)); else memcpy((p), &(var), sizeof(var)); \
        (p) += sizeof(var); } while (0)

#endif
