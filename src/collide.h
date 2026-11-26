/*
 * FRONTIER - collision: projectiles against ships.
 *
 * Each frame the targets are put into a coarse grid (128-pixel cells folded
 * onto 64 buckets). A projectile then only looks at the one bucket it is in,
 * so the cost grows with the number of projectiles, not projectiles x ships.
 *
 * Movement is at most about 16 pixels per frame, so every projectile tests two
 * sample points (where it is, and half a step back). With the smallest target
 * radius of 7 pixels no shot can pass through a ship between frames.
 */
#ifndef FRONTIER_COLLIDE_H
#define FRONTIER_COLLIDE_H

#include "common.h"

#define COLLIDE_MAX_HITTABLES 48
#define PLAYER_ID 255

/* Target ids: drones are 0..31, enemy ships ENEMY_ID_BASE.., the player PLAYER_ID.
 * Called for every hit with the damage and the hit point (whole world pixels). */
#define ENEMY_ID_BASE 32
/* The team of the projectile that caused the hit being reported (read it in the HitFn). */
extern int collide_shooter_team;

typedef void (*HitFn)(int id, int damage, int hit_x, int hit_y);

void collide_begin(void);
/* x, y are whole pixels. team decides which projectiles can hit it. */
void collide_add(s32 x, s32 y, int radius, int team, int id);
void collide_projectiles(HitFn on_hit);

int  collide_tests(void);   /* distance tests done last frame */

#endif
