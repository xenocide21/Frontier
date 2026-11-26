#include <stddef.h>
#include <stdio.h>
#include <psxgpu.h>
#include <psxgte.h>
#include "collide.h"
#include "enemy.h"
#include "escort.h"
#include "outfit.h"
#include "rng.h"
#include "weapon.h"
#include "galaxy.h"
#include "world.h"

typedef struct {
    Ship ship;
    u8   used;
    u8   fidx;         /* index into the fleet arrays */
} Escort;

static Escort esc[MAX_ESCORTS];
static int lost_count;
static const char *esc_msg;     /* a promotion or a loss, for the HUD (M15b) */
int escort_threat = -1;   /* enemy slot that recently shot the player (set by main) */

void escort_init(void) {
    for (int k = 0; k < MAX_ESCORTS; k++) esc[k].used = 0;
    lost_count = 0;
}

const Ship *escort_ship(int k) {
    if (k < 0 || k >= MAX_ESCORTS || !esc[k].used || !ship_alive(&esc[k].ship)) return NULL;
    return &esc[k].ship;
}

void escort_refit_all(void) {
    for (int k = 0; k < MAX_ESCORTS; k++)
        if (esc[k].used) ship_set_layout(&esc[k].ship, &fleet_layout[esc[k].fidx]);
}

int escort_count(void) {
    int n = 0;
    for (int k = 0; k < MAX_ESCORTS; k++) n += esc[k].used;
    return n;
}

int escort_take_lost(void) { int n = lost_count; lost_count = 0; return n; }

void escort_scored(int k) {
    if (k < 0 || k >= MAX_ESCORTS || !esc[k].used) return;
    const char *m = fleet_scored(esc[k].fidx);
    if (m) esc_msg = m;
}
const char *escort_take_message(void) { const char *m = esc_msg; esc_msg = 0; return m; }

static void place_beside(Ship *s, const Ship *pl, int k) {
    int a = (pl->angle + 2048 + (k - 1) * 600) & ANGLE_MASK;
    s->x = pl->x + isin(a) * 90;
    s->y = pl->y - icos(a) * 90;
    s->vx = pl->vx;
    s->vy = pl->vy;
    s->angle = pl->angle;
}

/* Where a garrison waits: around the station. */
static void garrison_post(int k, s32 *gx, s32 *gy) {
    const NavObject *st = world_nav(NAV_STATION);
    if (world_empty()) { static const NavObject origin = { "", 0, 0, 0, 0, 0 }; st = &origin; }   /* empty space: hold the middle */
    int a = (k * 1365) & ANGLE_MASK;
    *gx = TO_FP(st->x) + isin(a) * 160;
    *gy = TO_FP(st->y) - icos(a) * 160;
}

/* Matches the active escorts to the fleet list: wing ships with you, then the
 * ships of any squadron sitting in this system (its garrison). */
static void sync(const Ship *pl) {
    int want[MAX_ESCORTS], n = 0;
    for (int i = 0; i < fleet_count && n < MAX_WING; i++)
        if (fleet_sq[i] == 0 && fleet_stance[i] != STANCE_HANGAR && fleet_here(i)) want[n++] = i;
    for (int k = n; k < MAX_WING; k++) want[k] = -1;
    n = MAX_WING;
    for (int i = 0; i < fleet_count && n < MAX_ESCORTS; i++)
        if (fleet_sq[i] != 0 && sq_order[fleet_sq[i]] != SQ_MOVE && fleet_here(i)) want[n++] = i;

    for (int k = 0; k < MAX_ESCORTS; k++) {
        if (k >= n || want[k] < 0) { esc[k].used = 0; continue; }
        if (esc[k].used && esc[k].ship.def_index == fleet[want[k]]) {
            if (esc[k].ship.layout != &fleet_layout[want[k]])
                ship_set_layout(&esc[k].ship, &fleet_layout[want[k]]);   /* the hangar order changed */
            esc[k].fidx = (u8) want[k];
            continue;
        }
        ship_init(&esc[k].ship, fleet[want[k]], TEAM_ALLY);
        ship_set_layout(&esc[k].ship, &fleet_layout[want[k]]);    /* its own fit */
        if (k < MAX_WING) place_beside(&esc[k].ship, pl, k);
        else {
            garrison_post(k, &esc[k].ship.x, &esc[k].ship.y);
            esc[k].ship.angle = (k * 1365) & ANGLE_MASK;
        }
        esc[k].fidx = (u8) want[k];
        esc[k].used = 1;
    }
}

static int steer(int f, int r) {
    int ar = r < 0 ? -r : r;
    if (f < 0)      return r >= 0 ? 1 : -1;
    if (ar * 8 > f) return r >= 0 ? 1 : -1;
    return 0;
}

/* Turns toward a point (dx, dy pixels away) and returns the distance. */
static int aim(const Ship *s, int dx, int dy, ShipControls *c, int *aligned) {
    s32 sn = isin(s->angle), cs = icos(s->angle);
    int f = (dx * sn - dy * cs) >> FP_SHIFT;
    int r = (dx * cs + dy * sn) >> FP_SHIFT;
    c->turn = (s8) steer(f, r);
    *aligned = f > 0 && (r < 0 ? -r : r) * 10 < f;
    return (int) approx_len(dx, dy);
}

static int anim_frame;
void escort_update(const Ship *pl, int target_slot) {
    anim_frame++;
    sync(pl);

    for (int k = 0; k < MAX_ESCORTS; k++) {
        Escort *e = &esc[k];
        if (!e->used) continue;
        Ship *s = &e->ship;
        ShipControls c = { 0 };

        if (!ship_disabled(s)) {
            int stance = fleet_stance[e->fidx];
            const Ship *tgt = NULL;
            if (k >= MAX_WING) {                            /* garrison: anything hostile near it */
                tgt = enemies_ship(enemies_nearest_hostile(s->x, s->y, 700));
            } else if (stance == STANCE_ATTACK) {
                tgt = enemies_ship(target_slot);            /* only what you are attacking */
            } else if (stance == STANCE_GUARD) {
                tgt = enemies_ship(escort_threat);          /* whoever is shooting at you */
            }
            if (tgt && ship_disabled(tgt)) tgt = NULL;

            int aligned = 0;
            int rank = fleet_rank(e->fidx);                 /* the pilot's skill (M15b) */
            if (tgt && rank >= RANK_VETERAN && s->hull * 100 < (int) s->def.hull_max * 30 && (e->fidx + (int) (s->x >> 16)) & 1)
                tgt = NULL;                                 /* a veteran knows when to break off and regroup */
            if (tgt) {
                int dx = (int) FROM_FP(tgt->x - s->x), dy = (int) FROM_FP(tgt->y - s->y);
                int d = aim(s, dx, dy, &c, &aligned);
                int hold = 150 + 30 * rank;                 /* better pilots keep a smarter distance */
                if (d > hold + 80) { c.fwd = FP_ONE; c.boost = rank >= RANK_PILOT && d > 500; }
                else if (d < hold) c.fwd = -FP_ONE / 2;
                if (rank >= RANK_ACE && d < hold + 80) c.side = (s16) (((anim_frame >> 6) & 1) ? FP_ONE / 2 : -FP_ONE / 2);   /* aces jink */
                c.tgt = tgt;
                if (d < 380 + 50 * rank) c.fire1 = 1;       /* each gun fires only if it can bear */
            } else if (k >= MAX_WING) {
                s32 gx, gy;
                garrison_post(k, &gx, &gy);
                int dx = (int) FROM_FP(gx - s->x), dy = (int) FROM_FP(gy - s->y);
                int d = aim(s, dx, dy, &c, &aligned);
                if (d > 200) c.fwd = FP_ONE; else if (d > 60) c.fwd = FP_ONE / 3;
                else c.turn = 1;                            /* on post: slowly turns, watching */
            } else {
                /* Formation: a spot behind and to the side of the player. */
                int a = (pl->angle + 2048 + (k - 1) * 600) & ANGLE_MASK;
                s32 gx = pl->x + isin(a) * 90, gy = pl->y - icos(a) * 90;
                int dx = (int) FROM_FP(gx - s->x), dy = (int) FROM_FP(gy - s->y);
                int d = aim(s, dx, dy, &c, &aligned);
                if (d > 1800) { place_beside(s, pl, k); }
                else if (d > 260) { c.fwd = FP_ONE; c.boost = d > 600; }
                else if (d > 70)  { c.fwd = FP_ONE / 2; }
                else {
                    /* On station: face the way the player faces. */
                    int diff = (pl->angle - s->angle) & ANGLE_MASK;
                    if (diff > 80 && diff < 4016) c.turn = diff < 2048 ? 1 : -1;
                }
            }
        }
        ship_update(s, &c);
        proj_spawn_owner = 200 + k;                      /* its kills are credited to its pilot */
        ship_fire(s, &c);
        proj_spawn_owner = 0;
    }
}

void escort_place_near(const Ship *pl) {
    for (int k = MAX_WING; k < MAX_ESCORTS; k++) esc[k].used = 0;   /* a new system: its own garrison */
    for (int k = 0; k < MAX_WING; k++)
        if (esc[k].used) place_beside(&esc[k].ship, pl, k);
}

void escort_add_hittables(void) {
    for (int k = 0; k < MAX_ESCORTS; k++) {
        if (!esc[k].used) continue;
        collide_add(FROM_FP(esc[k].ship.x), FROM_FP(esc[k].ship.y),
                    ship_looks[esc[k].ship.def_index].radius, TEAM_ALLY, ESCORT_ID_BASE + k);
    }
}

void escort_damage(int id, int damage, int hx, int hy) {
    int k = id - ESCORT_ID_BASE;
    if (k < 0 || k >= MAX_ESCORTS || !esc[k].used) return;
    if (ship_damage(&esc[k].ship, damage, hx, hy) == DMG_DESTROYED) {
        static char lost[48];
        snprintf(lost, sizeof lost, "%s WENT DOWN WITH THE %s", fleet_pname[esc[k].fidx], ship_looks[esc[k].ship.def_index].name);
        esc_msg = lost;
        fleet_remove(esc[k].fidx);           /* the ship is gone for good */
        esc[k].used = 0;
        lost_count++;
    }
}

void escort_draw(s32 cx, s32 cy, int frame) {
    for (int k = 0; k < MAX_ESCORTS; k++)
        if (esc[k].used) ship_draw(&esc[k].ship, cx, cy, frame);
}
