/*
 * FRONTIER - navigation computer, autopilot and the jump transition.
 *
 * Square cycles the navigation target (station, planet, none). A
 * pointer on screen shows where it is and how far. Circle engages the
 * autopilot, which flies a straight course at up to three times the ship's top
 * speed and brakes to a stop at the destination. The autopilot hands control
 * back when:
 *   - the pilot touches the controls or fires,
 *   - an active pirate comes within scanner range (600 px),
 *   - the ship is hit, disabled or destroyed.
 *
 * Hyperspace (from Milestone 12c) works from anywhere in open space, away from
 * planets and stations (world_jump_lock). A jump runs in phases:
 *   STOP    the ship brakes to a standstill
 *   CHARGE  the hyperdrive charges (JUMP_CHARGE_FRAMES). For the first half the
 *           ship can turn and shoot but is a sitting duck: thrusting, boosting
 *           or pressing Triangle again aborts the jump. From half charge the
 *           ship swings toward the destination and speeds up faster and
 *           faster until it streaks off the screen (the camera stays put);
 *           only Triangle can still abort.
 *   JUMP    the transition plays; at its midpoint the caller moves the ship to
 *           the destination system. The ship is safe during the transition.
 */
#ifndef FRONTIER_NAV_H
#define FRONTIER_NAV_H

#include "common.h"
#include "ship.h"

#define SCANNER_RANGE 600

enum { NAV_EVENT_NONE = 0 };

typedef struct {
    int  target;         /* -1 = none, else a NAV_* index in the current system */
    int  autopilot;      /* 1 = engaged */
    int  msg_timer;      /* frames the message stays on screen */
    char msg[72];
} Nav;

void nav_init(Nav *n);
void nav_cycle(Nav *n);
void nav_toggle_autopilot(Nav *n, const Ship *s);

/* Call before ship_update. If the autopilot is engaged it takes over the ship's
 * heading and velocity and sets c->cruise. */
int  nav_prepare(Nav *n, Ship *s, ShipControls *c);

void nav_message(Nav *n, const char *text);
int  nav_distance(const Nav *n, const Ship *s);   /* pixels to the target, or -1 */

/* Pointer to the nav target (edge arrow when off screen) and its label. */
void nav_draw_pointer(const Nav *n, const Ship *s, s32 cam_x, s32 cam_y);

/* Hyperspace jump. */
#define JUMP_CHARGE_FRAMES 150          /* 2.5 s: one warp drive of the hull's size (more drives: faster) */
enum { NJ_NONE = 0, NJ_FIRE, NJ_ARRIVE, NJ_ABORT };

void nav_jump_begin(Nav *n, int heading, int charge_frames);   /* STOP phase; heading: toward the destination;
                                                               * charge_frames from the warp drives */
int  nav_jump_departing(void);          /* charge past half: the ship is accelerating away */
int  nav_jump_heading(void);
void nav_jump_heading_set(int a);     /* dev warp: arrive flying this way */
int  nav_jump_camera_hold(void);        /* 1 while the camera should stay put (ship leaving) */
void nav_jump_abort(Nav *n, const char *why);
int  nav_jump_charging(void);           /* in STOP or CHARGE */
int  nav_jump_active(void);             /* in the transition (ship safe, no control) */
/* Call after the controls are read and before ship_update. In STOP and CHARGE
 * it brakes the ship and blocks thrust; thrust or boost input aborts. Returns
 * NJ_FIRE when the drive is charged (spend the fuel now), NJ_ARRIVE on the
 * white frame (move the ship to the new system), NJ_ABORT when cancelled. */
int  nav_jump_update(Nav *n, Ship *s, ShipControls *c);
void nav_jump_draw(void);

#endif
