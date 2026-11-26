#include <stdio.h>
#include <string.h>
#include <psxgpu.h>
#include <psxgte.h>

#include "enemy.h"
#include "gfx.h"
#include "nav.h"
#include "world.h"

#define CRUISE_MULT       3     /* autopilot top speed as a multiple of the ship's */
#define BRAKE_FRAMES      40    /* speed = distance / this, so it eases in */
#define MESSAGE_FRAMES    180
#define JUMP_FRAMES       150
#define JUMP_MIDPOINT     75

void nav_init(Nav *n) {
    n->target = -1;
    n->autopilot = 0;
    n->msg_timer = 0;
    n->msg[0] = 0;
}

void nav_message(Nav *n, const char *text) {
    strncpy(n->msg, text, sizeof n->msg - 1);
    n->msg[sizeof n->msg - 1] = 0;
    n->msg_timer = MESSAGE_FRAMES;
}

void nav_cycle(Nav *n) {
    n->target++;
    if (n->target >= world_nav_count()) n->target = -1;
    n->autopilot = 0;   /* changing the destination always disengages */
}

int nav_distance(const Nav *n, const Ship *s) {
    if (n->target < 0) return -1;
    const NavObject *o = world_nav(n->target);
    return (int) approx_len((int) FROM_FP(s->x) - o->x, (int) FROM_FP(s->y) - o->y);
}

void nav_toggle_autopilot(Nav *n, const Ship *s) {
    if (n->autopilot) {
        n->autopilot = 0;
        nav_message(n, "AUTOPILOT OFF");
    } else if (n->target < 0) {
        nav_message(n, "NO NAV TARGET: PRESS SQUARE");
    } else if (!ship_alive(s) || ship_disabled(s)) {
        nav_message(n, "ENGINES DOWN");
    } else if (enemies_hostiles_near(s->x, s->y, SCANNER_RANGE) > 0) {
        nav_message(n, "HOSTILES IN RANGE");
    } else {
        n->autopilot = 1;
        nav_message(n, "AUTOPILOT ENGAGED");
    }
}

/* True if the pilot is asking the ship to do anything. */
static int pilot_input(const ShipControls *c) {
    return c->turn || c->fwd || c->side || c->boost || c->fire1 || c->fire2;
}

static int recently_hit(const Ship *s) {
    return s->flash || s->shield_flash[0] >= 9 || s->shield_flash[1] >= 9 ||
           s->shield_flash[2] >= 9 || s->shield_flash[3] >= 9;
}

int nav_prepare(Nav *n, Ship *s, ShipControls *c) {
    if (n->msg_timer > 0) n->msg_timer--;
    if (!n->autopilot) return NAV_EVENT_NONE;

    /* Reasons to hand control back. */
    if (!ship_alive(s) || ship_disabled(s)) { n->autopilot = 0; return NAV_EVENT_NONE; }
    if (pilot_input(c))        { n->autopilot = 0; nav_message(n, "AUTOPILOT OFF"); return NAV_EVENT_NONE; }
    if (recently_hit(s))       { n->autopilot = 0; nav_message(n, "AUTOPILOT OFF: UNDER FIRE"); return NAV_EVENT_NONE; }
    if (enemies_hostiles_near(s->x, s->y, SCANNER_RANGE) > 0) {
        n->autopilot = 0;
        nav_message(n, "AUTOPILOT OFF: HOSTILE CONTACT");
        return NAV_EVENT_NONE;
    }

    const NavObject *o = world_nav(n->target);
    int dx = o->x - (int) FROM_FP(s->x);
    int dy = o->y - (int) FROM_FP(s->y);
    int dist = (int) approx_len(dx, dy);

    if (dist < o->arrive) {
        n->autopilot = 0;
        char buf[40];
        snprintf(buf, sizeof buf, "ARRIVED: %s", o->name);
        nav_message(n, buf);
        return NAV_EVENT_NONE;
    }

    const ShipDef *d = &s->def;
    s32 sn = isin(s->angle);
    s32 cs = icos(s->angle);
    int f = (dx * sn - dy * cs) >> FP_SHIFT;   /* target ahead of the nose */
    int r = (dx * cs + dy * sn) >> FP_SHIFT;   /* target to the right */

    /* Turn toward the target at twice the normal rate. */
    int ar = r < 0 ? -r : r;
    int turn = 0;
    if (f < 0 || ar * 8 > f) turn = (r >= 0) ? 1 : -1;
    s->angle = (s->angle + turn * 2 * d->turn_rate) & ANGLE_MASK;
    sn = isin(s->angle);
    cs = icos(s->angle);

    /* Speed: cruise speed, eased down as the destination nears, and reduced
     * while the ship is not yet pointing at it. */
    s32 cruise = (s32) d->max_speed * CRUISE_MULT;
    s32 want = ((s32) dist << FP_SHIFT) / BRAKE_FRAMES;
    if (want > cruise) want = cruise;
    int align = f > 0 ? f : 0;
    if (align > dist) align = dist;
    want = (want * align) / (dist > 0 ? dist : 1);

    s32 vx_want = (sn * want) >> FP_SHIFT;
    s32 vy_want = (-cs * want) >> FP_SHIFT;
    s->vx += (vx_want - s->vx) >> 3;
    s->vy += (vy_want - s->vy) >> 3;

    c->cruise = 1;
    return NAV_EVENT_NONE;
}

/* ------------------------------------------------------------------------ */
/* Pointer to the nav target                                                 */
/* ------------------------------------------------------------------------ */

void nav_draw_pointer(const Nav *n, const Ship *s, s32 cam_x, s32 cam_y) {
    if (n->target < 0) return;
    const NavObject *o = world_nav(n->target);

    int sx = SCREEN_W / 2 + o->x - (int) FROM_FP(cam_x);
    int sy = SCREEN_H / 2 + o->y - (int) FROM_FP(cam_y);
    int dist = nav_distance(n, s);
    char label[40];
    snprintf(label, sizeof label, "%s %d", o->name, dist);

    const int hw = SCREEN_W / 2 - 40, hh = SCREEN_H / 2 - 56;
    int vx = sx - SCREEN_W / 2, vy = sy - SCREEN_H / 2;
    int on_screen = (abs32(vx) < hw) && (abs32(vy) < hh);

    if (on_screen) {
        /* A small marker above the object, with the label under it. */
        int top = sy - o->radius - 16;
        if (o->kind == KIND_PLANET) top = sy - 40;   /* inside the big disc */
        TILE *t = (TILE *) gfx_prim(LAYER_BARS, sizeof(TILE));
        if (t) {
            setTile(t);
            setXY0(t, sx - 4, top);
            setWH(t, 8, 6);
            setRGB0(t, 90, 255, 200);
        }
        gfx_text(sx - (int) strlen(label) * 4, top + 8, LAYER_BARS, label);
        return;
    }

    /* Off screen: an arrow on the edge of the view pointing toward it. */
    int ex, ey;
    if (abs32(vx) * hh >= abs32(vy) * hw) {
        ex = SCREEN_W / 2 + (vx > 0 ? hw : -hw);
        ey = SCREEN_H / 2 + (vy * hw) / abs32(vx);
    } else {
        ey = SCREEN_H / 2 + (vy > 0 ? hh : -hh);
        ex = SCREEN_W / 2 + (vx * hh) / abs32(vy);
    }

    int len = (int) approx_len(vx, vy);
    if (len < 1) len = 1;
    int ux = (vx * 256) / len, uy = (vy * 256) / len;   /* unit vector, 8-bit fraction */
    POLY_F3 *arrow = (POLY_F3 *) gfx_prim(LAYER_BARS, sizeof(POLY_F3));
    if (arrow) {
        setPolyF3(arrow);
        setXY3(arrow,
               ex + (ux * 12) / 256,               ey + (uy * 12) / 256,
               ex - (ux * 6) / 256 - (uy * 7) / 256, ey - (uy * 6) / 256 + (ux * 7) / 256,
               ex - (ux * 6) / 256 + (uy * 7) / 256, ey - (uy * 6) / 256 - (ux * 7) / 256);
        setRGB0(arrow, 90, 255, 200);
    }

    int tx = ex - (int) strlen(label) * 4;
    int ty = ey + (uy > 0 ? -22 : 14);
    if (tx < 8) tx = 8;
    if (tx > SCREEN_W - 8 - (int) strlen(label) * 8) tx = SCREEN_W - 8 - (int) strlen(label) * 8;
    gfx_text(tx, ty, LAYER_BARS, label);
}

/* ------------------------------------------------------------------------ */
/* Jump transition                                                           */
/* ------------------------------------------------------------------------ */

enum { PH_NONE = 0, PH_STOP, PH_CHARGE, PH_JUMP };
static int phase, jump_timer, charge, jump_heading, charge_len = JUMP_CHARGE_FRAMES;

#define DEPART_TOP_SPEED (16 << FP_SHIFT)   /* px per frame at full charge: gone in a blink */

void nav_jump_begin(Nav *n, int heading, int charge_frames) {
    jump_heading = heading & ANGLE_MASK;
    charge_len = charge_frames > 30 ? charge_frames : 30;
    phase = PH_STOP;
    charge = 0;
    jump_timer = 0;
    n->autopilot = 0;
    nav_message(n, "ALL STOP: PREPARING TO JUMP");
}

void nav_jump_abort(Nav *n, const char *why) {
    if (phase != PH_STOP && phase != PH_CHARGE) return;
    phase = PH_NONE;
    nav_message(n, why);
}

int nav_jump_charging(void) { return phase == PH_STOP || phase == PH_CHARGE; }
int nav_jump_active(void)   { return phase == PH_JUMP; }
int nav_jump_departing(void) { return phase == PH_CHARGE && charge >= charge_len / 2; }
int nav_jump_heading(void)   { return jump_heading; }
void nav_jump_heading_set(int a) { jump_heading = a; }
int nav_jump_camera_hold(void) {
    return nav_jump_departing() || (phase == PH_JUMP && jump_timer < JUMP_MIDPOINT);
}

int nav_jump_update(Nav *n, Ship *s, ShipControls *c) {
    if (phase == PH_NONE) return NJ_NONE;

    if (phase == PH_STOP || phase == PH_CHARGE) {
        if (!ship_alive(s) || ship_disabled(s)) { nav_jump_abort(n, "JUMP ABORTED: SHIP DISABLED"); return NJ_ABORT; }
        int half = charge_len / 2;
        if (phase == PH_CHARGE && charge >= half) {
            /* Departure: swing toward the destination and accelerate ever faster. */
            int diff = ((jump_heading - s->angle + 2048) & ANGLE_MASK) - 2048;
            int step = s->def.turn_rate * 2;
            if (diff > step) diff = step; else if (diff < -step) diff = -step;
            s->angle = (s->angle + diff) & ANGLE_MASK;
            s32 t = ((charge - half) << FP_SHIFT) / half;                 /* 0 .. 1 */
            s32 v = (((t * t) >> FP_SHIFT) * (DEPART_TOP_SPEED >> 8)) >> (FP_SHIFT - 8);
            if (v < 4096) v = 4096;
            s->vx = (isin(s->angle) * v) >> FP_SHIFT;
            s->vy = -(icos(s->angle) * v) >> FP_SHIFT;
            c->turn = 0; c->fwd = 0; c->side = 0; c->boost = 0;
            c->cruise = 1;                                                 /* velocity set here, no cap */
            if (++charge < charge_len) return NJ_NONE;
            phase = PH_JUMP;
            jump_timer = 1;
            nav_message(n, "JUMPING...");
            return NJ_FIRE;
        }
        if (c->fwd || c->side || c->boost)       { nav_jump_abort(n, "JUMP ABORTED"); return NJ_ABORT; }
        c->cruise = 0;
        /* Brake hard: lose 1/12 of the speed each frame, then snap to zero. */
        s->vx -= s->vx / 12;
        s->vy -= s->vy / 12;
        if (abs32(s->vx) < 64 && abs32(s->vy) < 64) s->vx = s->vy = 0;

        if (phase == PH_STOP) {
            if (s->vx == 0 && s->vy == 0) {
                phase = PH_CHARGE;
                charge = 0;
                nav_message(n, "HYPERDRIVE CHARGING");
            }
            return NJ_NONE;
        }
        if (++charge < charge_len) return NJ_NONE;
        phase = PH_JUMP;
        jump_timer = 1;
        nav_message(n, "JUMPING...");
        return NJ_FIRE;
    }

    /* PH_JUMP: the transition. The ship keeps whatever velocity it has: off
     * screen before the midpoint, its arrival speed after it. */
    memset(c, 0, sizeof *c);
    c->cruise = jump_timer < JUMP_MIDPOINT;      /* no drag or cap while departing */
    jump_timer++;
    int midpoint = (jump_timer == JUMP_MIDPOINT);   /* the caller moves the ship to the new system */
    if (jump_timer >= JUMP_FRAMES) { jump_timer = 0; phase = PH_NONE; }
    return midpoint ? NJ_ARRIVE : NJ_NONE;
}

/* The charge meter: a bar under the ship while stopping and charging. */
static void draw_charge(void) {
    int w = 160, x = SCREEN_W / 2 - w / 2, y = SCREEN_H / 2 + 52;
    int fill = phase == PH_CHARGE ? charge * (w - 4) / charge_len : 0;
    gfx_rect(x, y, w, 8, LAYER_BARS, 30, 30, 60);
    if (fill > 0) gfx_rect(x + 2, y + 2, fill, 4, LAYER_HUD, 150, 130, 255);
    const char *label = phase == PH_STOP ? "ALL STOP" : "HYPERDRIVE CHARGING";
    gfx_text(SCREEN_W / 2 - (int) strlen(label) * 4, y + 12, LAYER_HUD, label);
}

/* Streaks rushing outward from the middle, then a wipe to white at the
 * halfway point, then back. */
void nav_jump_draw(void) {
    if (phase == PH_STOP || phase == PH_CHARGE) { draw_charge(); return; }
    if (phase != PH_JUMP || jump_timer <= 0) return;

    int p = jump_timer < JUMP_MIDPOINT ? jump_timer : JUMP_FRAMES - jump_timer;   /* 0..75..0 */
    int cx = SCREEN_W / 2, cy = SCREEN_H / 2;

    /* Wipe: grey rising to white in the last stretch before the midpoint. */
    if (p > 50) {
        int v = (p - 50) * 10;
        if (v > 255) v = 255;
        TILE *w = (TILE *) gfx_prim(LAYER_BARS, sizeof(TILE));
        if (w) {
            setTile(w);
            setXY0(w, 0, 0);
            setWH(w, SCREEN_W, SCREEN_H);
            setRGB0(w, v, v, v);
        }
    }

    for (int i = 0; i < 28; i++) {
        int a = (i * 146 + (i * i) * 11) & ANGLE_MASK;
        int r_in  = 24 + (p * p) / 24 + (i & 3) * 6;
        int len   = 30 + p * 4;
        if (len > 400) len = 400;
        int r_out = r_in + len;
        if (r_out > 600) r_out = 600;
        if (r_in > r_out) continue;

        LINE_F2 *l = (LINE_F2 *) gfx_prim(LAYER_BARS + 0, sizeof(LINE_F2));
        if (!l) return;
        setLineF2(l);
        setXY2(l,
               cx + ((isin(a) * r_in)  >> FP_SHIFT), cy - ((icos(a) * r_in)  >> FP_SHIFT),
               cx + ((isin(a) * r_out) >> FP_SHIFT), cy - ((icos(a) * r_out) >> FP_SHIFT));
        int b = 140 + p;
        if (b > 255) b = 255;
        setRGB0(l, b - 40, b - 20, 255);
    }
}
