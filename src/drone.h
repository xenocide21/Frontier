/*
 * FRONTIER - target drones.
 *
 * Simple moving targets used to test weapons and collision. They drift around
 * the player, flash when hit, explode when destroyed and come back a little
 * later. Normal mode: a few passive drones (they do not shoot). Stress mode:
 * 32 drones close to the player, each firing in random directions to fill the
 * projectile pool, so the frame time can be measured.
 */
#ifndef FRONTIER_DRONE_H
#define FRONTIER_DRONE_H

#include "common.h"

#define MAX_DRONES          32
#define DRONE_NORMAL_COUNT  6
#define DRONE_RADIUS        8

void drones_init(void);
void drones_set_mode(int stress, s32 player_x, s32 player_y);
void drones_update(s32 player_x, s32 player_y, int stress);

/* Adds every living drone to the collision grid. */
void drones_add_hittables(void);
void drones_damage(int id, int damage);
void drones_draw(s32 cam_x, s32 cam_y);

int drones_alive(void);
int drones_target(void);    /* how many the current mode wants */
int drones_kills(void);

#endif
