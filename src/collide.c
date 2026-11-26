#include "collide.h"
#include "fx.h"
#include "weapon.h"

#define CELL_SHIFT   7              /* 128-pixel cells */
#define BUCKETS      64             /* 8 x 8 cells folded together */
#define MAX_NODES    (COLLIDE_MAX_HITTABLES * 4)   /* a ship touches up to 4 cells */
#define HIT_MARGIN   2              /* projectiles are slightly "fat" */

int collide_shooter_team;

typedef struct { s32 x, y; s16 r; u8 team, id; } Hittable;
typedef struct { s16 next; u8 hittable; u8 pad; } Node;

static Hittable hits[COLLIDE_MAX_HITTABLES];
static Node     nodes[MAX_NODES];
static s16      heads[BUCKETS];
static int      hit_count, node_count, tests;

static inline int bucket_of(int cx, int cy) {
    return ((cy & 7) << 3) | (cx & 7);
}

void collide_begin(void) {
    hit_count = 0;
    node_count = 0;
    tests = 0;
    for (int i = 0; i < BUCKETS; i++) heads[i] = -1;
}

void collide_add(s32 x, s32 y, int radius, int team, int id) {
    if (hit_count >= COLLIDE_MAX_HITTABLES) return;
    int idx = hit_count++;
    hits[idx].x = x;  hits[idx].y = y;
    hits[idx].r = (s16) radius;
    hits[idx].team = (u8) team;
    hits[idx].id = (u8) id;

    int cx0 = (x - radius) >> CELL_SHIFT, cx1 = (x + radius) >> CELL_SHIFT;
    int cy0 = (y - radius) >> CELL_SHIFT, cy1 = (y + radius) >> CELL_SHIFT;
    for (int cy = cy0; cy <= cy1; cy++) {
        for (int cx = cx0; cx <= cx1; cx++) {
            if (node_count >= MAX_NODES) return;
            int b = bucket_of(cx, cy);
            nodes[node_count].hittable = (u8) idx;
            nodes[node_count].next = heads[b];
            heads[b] = (s16) node_count;
            node_count++;
        }
    }
}

int collide_tests(void) { return tests; }

void collide_projectiles(HitFn on_hit) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile *p = &proj_pool[i];
        if (!p->active) continue;
        const WeaponDef *w = &weapon_defs[p->weapon];

        int hit_id = -1, hx = 0, hy = 0;
        for (int s = 0; s < 2 && hit_id < 0; s++) {
            s32 px = FROM_FP(s ? p->x - (p->vx >> 1) : p->x);
            s32 py = FROM_FP(s ? p->y - (p->vy >> 1) : p->y);
            int b = bucket_of(px >> CELL_SHIFT, py >> CELL_SHIFT);

            for (int n = heads[b]; n >= 0; n = nodes[n].next) {
                const Hittable *h = &hits[nodes[n].hittable];
                if ((h->team == TEAM_ALLY ? TEAM_PLAYER : h->team) == (p->team == TEAM_ALLY ? TEAM_PLAYER : p->team)) continue;
                tests++;
                s32 dx = px - h->x, dy = py - h->y;
                s32 rr = h->r + HIT_MARGIN;
                if (dx * dx + dy * dy <= rr * rr) {
                    hit_id = h->id;
                    hx = px;  hy = py;
                    break;
                }
            }
        }
        if (hit_id < 0) continue;

        collide_shooter_team = p->team;
        proj_hit_weapon = p->weapon;
        proj_hit_owner = p->owner;
        on_hit(hit_id, w->damage * (p->tier >= 3 ? 190 : p->tier == 2 ? 140 : 100) / 100, hx, hy);
        proj_hit_weapon = WEAPON_NONE;
        proj_hit_owner = 0;
        p->active = 0;

        if (w->kind == WK_ROCKET) {
            fx_burst(TO_FP(hx), TO_FP(hy), 14, 9000, 26, FX_FIRE);
            fx_burst(TO_FP(hx), TO_FP(hy),  6, 12000, 16, FX_SPARK);
        } else {
            fx_burst(TO_FP(hx), TO_FP(hy), 4, 6000, 10, FX_SPARK);
        }
    }
}
