/*
 * FRONTIER - the star system: backdrop, station and planet.
 *
 * Stars and beacons are not stored: every one comes from a hash of its cell.
 * The station, planet and jump point sit at fixed positions (data, below in
 * world.c) in one continuous space, with no loading between them.
 */
#ifndef FRONTIER_WORLD_H
#define FRONTIER_WORLD_H

#include "common.h"
#include "ship.h"

/* Things the navigation computer and autopilot can fly to in the current
 * system: the station and the planet. Positions are whole world pixels.
 * (Hyperspace no longer uses jump rings: see world_jump_lock.) */
enum { NAV_STATION = 0, NAV_PLANET, NAV_MAX };
enum { KIND_STATION = 0, KIND_PLANET };

typedef struct {
    const char *name;     /* upper case: the debug font is upper case */
    s16 x, y;
    s16 radius;           /* size of the object, pixels */
    s16 arrive;           /* autopilot stops this close to the centre, pixels */
    u8  kind;
} NavObject;

#define STATION_RANGE 90  /* docking range of the station, pixels */

const NavObject *world_nav(int index);
int  world_nav_count(void);            /* objects in the current system */
int  world_empty(void);                /* this system has no station and no planet (M16a) */

/* Loads a system's objects (station, planet) from the galaxy. */
void world_set_system(int system_id);

/* Hyperspace lock: no jumping this close to a planet or station.
 * Returns NAV_PLANET or NAV_STATION for the object in the way, or -1 if clear;
 * *need (if not NULL) gets how many more pixels the ship must fly out. */
#define JUMP_LOCK_STATION 500          /* pixels from the station */
#define JUMP_LOCK_PLANET  500          /* pixels beyond the planet's surface */
int  world_jump_lock(s32 x, s32 y, int *need);

/* Where a ship arriving from system `from` appears in the current system. */
void world_arrival_point(int from, int *x, int *y);

void world_draw_stars(s32 cam_x, s32 cam_y);
void world_draw_planet(s32 off_x, s32 off_y);    /* the title screen backdrop */
void world_draw_objects(s32 cam_x, s32 cam_y, int frame);

#endif
