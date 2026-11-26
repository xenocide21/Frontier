/*
 * FRONTIER - places that can be docked with.
 *
 * One entry per station or planet: its description, its interior picture and
 * the services it offers. The dock menu lists only the services a place has, so
 * not every place sells ships or upgrades. When the galaxy exists (Milestone 9)
 * this table becomes data loaded per system.
 */
#ifndef FRONTIER_PLACE_H
#define FRONTIER_PLACE_H

#include "common.h"

enum {
    SVC_REPAIR   = 1 << 0,
    SVC_SHIPYARD = 1 << 1,
    SVC_TRADE    = 1 << 2,
    SVC_LOUNGE   = 1 << 3,
    SVC_GUILD    = 1 << 4,     /* mercenary guild: only where ships are sold */
    SVC_BLACK    = 1 << 5      /* black market: Syndicate systems (M13h) */
};

enum { ART_STATION = 0, ART_SPACEPORT };

#define PLACE_DESC_LINES 3

typedef struct {
    int         nav_index;                      /* NAV_* entry in world.c */
    u8          services;
    u8          art;
    const char *desc[PLACE_DESC_LINES];         /* up to 56 characters each */
} Place;

/* The place for a navigation object (NAV_STATION or NAV_PLANET) in the current
 * system, or NULL. */
const Place *place_for_nav(int nav_index);

/* Draws the interior picture inside the box x, y, w, h on layer z. frame
 * animates the lights. */
void place_set_ship(int mesh);   /* the ship shown on the pad */
void place_draw_art(const Place *p, int x, int y, int w, int h, int z, int frame);

#endif
