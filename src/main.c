#include <stdlib.h>
/*
 * FRONTIER - Milestone 12d: missile types, gun arcs and turrets.
 *
 * Milestone 12d: L1 picks the missile (rocket, seeker, ion); guns swing up to 30
 * degrees either side of the nose to lead the combat target; ships with turrets
 * (Wayfarer 1, Hauler 2) fire all round at the combat target while R2 is held.
 * Being shot makes the shooter your target (unless you already have an active
 * hostile targeted).
 *
 * Milestone 12c: radar, jump-from-anywhere hyperspace, UFS / FSA.
 *
 * Milestone 12c changes: a radar scope (top right, range set by the RADAR
 * upgrade); hyperspace no longer uses jump rings. Plot a course on the galaxy
 * map, fly clear of planets and stations, and press Triangle: the ship stops,
 * the drive charges, then it jumps. Thrusting while charging aborts the jump.
 * The factions are now the UFS and the FSA. Light ships have no retro-thrusters:
 * off the throttle they drift, and stopping means turning round and burning
 * (no reverse thrust). Heavy ships (the Hauler) brake by themselves. Flight
 * assist (Start menu, OPTIONS) adds gentle drag to every ship as an easy mode.
 *
 * Fly slowly into the station or over the planet and press Start to dock: a
 * docked ship is safe from pirates, and a menu offers ship status and repair.
 * The station shipyard sells ships and four module upgrades; pirate kills pay
 * credits. The trading post buys and sells 15 commodities; prices follow stock,
 * which follows the game clock. Four star systems are linked by hyperspace
 * routes: plot a course, then jump from open space, spending fuel and
 * several game days. Select opens the galaxy map,
 * where a course can be plotted. The lounge and the guild stay locked.
 * L1 + Start switches on a stress test (32 drones, 6 pirates, up to 128
 * projectiles) so the frame time can be read off the HUD.
 *
 * Controls (port 1, DualShock in analog mode; see the CONTROLS page of the Start menu):
 *   D-pad / left stick   turn and thrust; right stick strafes
 *   Cross                afterburner (hold)
 *   R2 / L2              fire all lasers and guns / fire the selected missile
 *   L1                   change missile type: rocket, seeker (homing), ion (disables)
 *   R1                   change combat target
 *   Square               cycle navigation target (station, planet)
 *   Triangle             dock, board a wreck, or jump along the plotted course
 *                        (press again while charging to cancel)
 *   Circle               autopilot on / off
 *   Left stick click     lock on to the target and follow it
 *   Select               galaxy map (pauses the game)
 *   Start                system menu: fleet stances, options (assist, test aids), controls
 */
#include <stdio.h>
#include <string.h>
#include <psxapi.h>
#include <psxgpu.h>
#include <psxgte.h>
#include <psxpad.h>

#include "audio.h"
#include "clock.h"
#include "collide.h"
#include "common.h"
#include "dock.h"
#include "drone.h"
#include "enemy.h"
#include "escort.h"
#include "sysmenu.h"
#include "faction.h"
#include "galaxy.h"
#include "salvage.h"
#include "scan.h"
#include "mission.h"
#include "save.h"
#include "title.h"
#include "model.h"
#include "fx.h"
#include "gfx.h"
#include "input.h"
#include "market.h"
#include "nav.h"
#include "outfit.h"
#include "radar.h"
#include "sprite.h"
#include "ship.h"
#include "weapon.h"
#include "world.h"
#include "conquest.h"
#include "veyr.h"
#include "story.h"

/* The camera looks this many frames ahead of the ship's velocity and eases
 * toward that point by 1/8 of the gap each frame. */
#define CAMERA_LOOKAHEAD_FRAMES 12
#define CAMERA_EASE_SHIFT 3

/* One NTSC frame is about 262 scanlines (horizontal blanks). */
#define SCANLINES_PER_FRAME 262


/* Targets farther than this are dropped. */
#define TARGET_RANGE 700

typedef struct { s32 x, y; } Camera;

static Ship ship;
static Nav  nav;
static int  lock_on;            /* L3: point at the target and follow it */
static int  wreck_slot = -1;   /* a disabled ship within boarding range */
static int  salvage_slot = -1; /* the wreck being salvaged */
static int  launch_guard;   /* frames of protection after launching */
static int  attacker = -1;  /* enemy slot that last hit the player, or -1 */
static int  threat_timer;   /* frames until guards stop chasing the last attacker */
static int  scooping;       /* the antimatter scoop is gathering fuel (M15a) */

static void camera_follow(Camera *cam, const Ship *s) {
    s32 tx = s->x + s->vx * CAMERA_LOOKAHEAD_FRAMES;
    s32 ty = s->y + s->vy * CAMERA_LOOKAHEAD_FRAMES;
    cam->x += (tx - cam->x) >> CAMERA_EASE_SHIFT;
    cam->y += (ty - cam->y) >> CAMERA_EASE_SHIFT;
}

/* The music follows what is happening (M19a): the dock's tune, the Veyr's when
 * they are near or this is their world, a fight's while anything hostile is
 * close (and for a few seconds after), or the quiet one. The hull alarm too. */
static void music_context(void) {
    static int combat_hold, veyr_hold, low;
    audio_listen((int) FROM_FP(ship.x), (int) FROM_FP(ship.y));
    if (ship.docked) { audio_music(SONG_DOCK); combat_hold = veyr_hold = 0; low = 0; return; }
    int h = ship_alive(&ship) ? enemies_nearest_hostile(ship.x, ship.y, 1000) : -1;
    if (h >= 0 || conquest_active()) {
        combat_hold = 360;
        if (h >= 0 && enemies_is_alien(h)) veyr_hold = 360;
    } else {
        if (combat_hold) combat_hold--;
        if (veyr_hold) veyr_hold--;
    }
    if (veyr_hold || galaxy_sys(galaxy_current())->owner == OWN_VEYR) audio_music(SONG_VEYR);
    else if (combat_hold) audio_music(SONG_COMBAT);
    else audio_music(SONG_FLIGHT);

    int now_low = ship_alive(&ship) && ship.hull * 4 < (int) ship.def.hull_max;
    if (now_low && !low) audio_sfx(SFX_ALARM);
    low = now_low;
}

/* Called by the collision code for every projectile hit. */
static void on_hit(int id, int damage, int hx, int hy) {
    if (id == PLAYER_ID) {
        if (!nav_jump_active() && launch_guard == 0) {
            ship_damage(&ship, damage, hx, hy);   /* safe in the jump and just after launch */
            if (proj_hit_owner > 0) {
                attacker = proj_hit_owner - 1;
                escort_threat = attacker;            /* guards go after them */
                threat_timer = 600;                  /* for ten seconds after the last hit */
            }
        }
    }
    else if (id >= ESCORT_ID_BASE)   escort_damage(id, damage, hx, hy);
    else if (id >= ENEMY_ID_BASE)    enemies_damage(id - ENEMY_ID_BASE, damage, hx, hy);
    else                             drones_damage(id, damage);
}

/* Draws one line of HUD text. Lines are 10 pixels apart. */

/* Four corner brackets around the targeted ship. */
static void draw_target_marker(const Ship *t, s32 cam_x, s32 cam_y, int frame) {
    int cx = SCREEN_W / 2 + FROM_FP(t->x - cam_x);
    int cy = SCREEN_H / 2 + FROM_FP(t->y - cam_y);
    int q = ship_looks[t->def_index].radius + 8;
    int arm = 5;
    int lum = (frame & 16) ? 255 : 180;

    for (int sy = -1; sy <= 1; sy += 2) {
        for (int sx = -1; sx <= 1; sx += 2) {
            int x = cx + sx * q, y = cy + sy * q;
            LINE_F2 *h = (LINE_F2 *) gfx_prim(LAYER_BARS, sizeof(LINE_F2));
            LINE_F2 *v = (LINE_F2 *) gfx_prim(LAYER_BARS, sizeof(LINE_F2));
            if (!h || !v) return;
            setLineF2(h);  setXY2(h, x, y, x - sx * arm, y);
            setLineF2(v);  setXY2(v, x, y, x, y - sy * arm);
            setRGB0(h, lum, lum, 60);
            setRGB0(v, lum, lum, 60);
        }
    }
}

/* ---- The flight HUD (redrawn in Milestone 14e) --------------------------- */

static void hud_text(int x, int y, u32 c, const char *t) {
    gfx_text_big(x, y, LAYER_HUD, 1, (int) (c >> 16 & 255), (int) (c >> 8 & 255), (int) (c & 255), t);
}
static void hud_centre(int y, u32 c, const char *t) { hud_text((SCREEN_W - (int) strlen(t) * 8) / 2, y, c, t); }

/* A dark glass plate behind a group of readouts (add it after them). */
static void hud_plate(int x, int y, int w, int h) {
    gfx_rect(x, y, w, 1, LAYER_HUD, 50, 84, 132);
    gfx_rect(x, y, 6, 2, LAYER_HUD, 150, 230, 255);
    gfx_blend(x, y, w, h, LAYER_HUD, GFX_HALF, 0x060C20, 0x020308);
}

static u32 health_colour(int v, int max) {
    int p = max > 0 ? v * 100 / max : 0;
    return p > 60 ? 0x50DC8C : p > 25 ? 0xFFC850 : 0xFF5A46;
}

static void hud_meter(int x, int y, const char *label, int v, int max, u32 c) {
    char t[24];
    hud_text(x, y, 0x96AACC, label);
    gfx_bar(x + 36, y, 110, 8, LAYER_HUD, v, max, (int) (c >> 16 & 255), (int) (c >> 8 & 255), (int) (c & 255));
    snprintf(t, sizeof t, "%d", v);
    hud_text(x + 152, y, 0xE2E8F4, t);
}

/* The four shield arcs as bars round a ship pip: lit by strength, flashing when hit. */
static void hud_shields(const Ship *s, int x, int y) {
    static const s8 bx[ARC_COUNT] = { 8, 34, 8, 0 }, by[ARC_COUNT] = { 0, 8, 34, 8 };
    for (int a = 0; a < ARC_COUNT; a++) {
        int max = s->def.shield_arc_max, v = max > 0 ? s->shield[a] * 255 / max : 0;
        int horiz = a == ARC_FRONT || a == ARC_BACK;
        int r = 20 + (s->shield_flash[a] ? 200 : 0), g = 40 + v * 150 / 255, b = 70 + v * 185 / 255;
        if (max == 0) { r = 60; g = 30; b = 30; }
        gfx_rect(x + bx[a], y + by[a], horiz ? 24 : 6, horiz ? 6 : 24, LAYER_HUD, r > 255 ? 255 : r, g, b);
    }
    gfx_tri(x + 20, y + 12, x + 27, y + 28, x + 13, y + 28, LAYER_HUD, 200, 210, 225);
}

static void draw_hud(const Ship *s, int stress, int dock_state, int jump_next, int target,
                     int vbl_per_frame, int cpu_lines) {
    char line[64];
    const ShipLook *look = &ship_looks[s->def_index];
    (void) cpu_lines;

    /* --- Top left: the ship --- */
    snprintf(line, sizeof line, "%s", look->name);
    hud_text(24, 20, 0x5ADCFF, line);
    snprintf(line, sizeof line, "%s SYSTEM", galaxy_sys(galaxy_current())->name);
    if ((int) (strlen(line) + strlen(look->name)) > 27)                 /* long names (M16a): drop "SYSTEM" */
        snprintf(line, sizeof line, "%s", galaxy_sys(galaxy_current())->name);
    hud_text(24 + 232 - (int) strlen(line) * 8, 20, 0x96AACC, line);
    if (ship_alive(s)) {
        hud_shields(s, 24, 34);
        hud_meter(74, 34, "HULL", (int) s->hull, s->def.hull_max, health_colour((int) s->hull, s->def.hull_max));
        hud_meter(74, 46, "ENGY", s->energy, s->energy_max, 0x5AB4FF);
        if (s->rounds_max > 0) hud_meter(74, 58, "AMMO", s->rounds, s->rounds_max, 0xFFBE46);
        else hud_text(74, 58, 0x687084, "AMMO  NO KINETIC GUNS");
        if (s->loadout->secondary != WEAPON_NONE) {
            snprintf(line, sizeof line, "%s %d", missile_name(s->msl_sel), s->msl[s->msl_sel]);
            hud_text(74, 70, 0xFFDC78, line);
        } else hud_text(74, 70, 0x687084, "NO MISSILE RACK");
        /* Systems: only named when hurt. */
        static const char *sn[SYS_COUNT] = { "ENG", "RCT", "GEN", "WPN" };
        int sx = 24;
        for (int k = 0; k < SYS_COUNT; k++) {
            if (s->sys[k] >= 100) continue;
            snprintf(line, sizeof line, "%s %d", sn[k], s->sys[k]);
            hud_text(sx, 84, health_colour(s->sys[k], 100), line);
            sx += 60;
        }
        hud_plate(16, 14, 248, sx > 24 ? 84 : 70);
    } else {
        hud_text(24, 40, 0xFF5A46, "SHIP DESTROYED");
        hud_plate(16, 14, 248, 44);
    }

    /* --- Top centre: what Triangle would do, or what is wrong --- */
    line[0] = 0;
    u32 pc = 0xFFE678;
    if (ship_alive(s) && ship_disabled(s)) {
        snprintf(line, sizeof line, "DISABLED! TOWED HOME IN %d", (600 - s->disabled_time) / 60 + 1);
        pc = 0xFF5A46;
    } else if (dock_state == DOCK_READY) {
        snprintf(line, sizeof line, "TRIANGLE: DOCK");
    } else if (dock_state == DOCK_TOO_FAST) {
        snprintf(line, sizeof line, "TOO FAST TO DOCK: SLOW BELOW %d", (DOCK_MAX_SPEED * 60) >> 12);
        pc = 0xFFA050;
    } else if (wreck_slot >= 0) {
        snprintf(line, sizeof line, "TRIANGLE: BOARD THE WRECK");
    } else if (jump_next >= 0 && !nav_jump_charging() && !nav_jump_active()) {
        int need;
        int lock = world_jump_lock(s->x, s->y, &need);
        if (!s->warp_ok) { snprintf(line, sizeof line, "NO WARP DRIVE BIG ENOUGH: REFIT"); pc = 0xFF5A46; }
        else if (lock >= 0) {
            snprintf(line, sizeof line, "TOO CLOSE TO %s TO JUMP: %d", lock == NAV_PLANET ? "THE PLANET" : "THE STATION", need);
            pc = 0xFFA050;
        } else
            snprintf(line, sizeof line, "TRIANGLE: JUMP TO %s (FUEL %d)",
                     galaxy_known(jump_next) ? galaxy_sys(jump_next)->name : "UNCHARTED",
                     galaxy_jump_fuel(galaxy_current(), jump_next) * s->warp_fuel_pct / 100);
    } else if (stress) {
        snprintf(line, sizeof line, "STRESS TEST (SHIP IMMUNE)");
    } else if (enemies_peace()) {
        snprintf(line, sizeof line, "PEACE MODE: NO PIRATES");
        pc = 0x96AACC;
    }
    if (line[0]) {
        int w = (int) strlen(line) * 8 + 24;
        hud_centre(104, pc, line);
        hud_plate((SCREEN_W - w) / 2, 98, w, 20);
    }
    if (conquest_hud(line, sizeof line)) {          /* M14g: the assault under way */
        int w = (int) strlen(line) * 8 + 24;
        hud_centre(128, 0xFF9050, line);
        hud_plate((SCREEN_W - w) / 2, 122, w, 20);
    }

    /* --- Under the scope: the target --- */
    const Ship *t = enemies_ship(target);
    if (t) {
        const ShipDef *td = &t->def;
        int tx = SCREEN_W - 24 - 128, ty = 172;
        const char *who = enemies_is_bounty(target) ? missions_bounty_name() :
                          enemies_is_alien(target) ? "VEYR" :
                          enemies_role(target) == ROLE_PIRATE ? "PIRATE" : enemies_role(target) == ROLE_MERCHANT ? "MERCHANT" :
                          enemies_role(target) == ROLE_DEFENDER ? "COLONY DEFENCE" :
                          t->team == TEAM_REBEL ? "FSA PATROL" : "UFS PATROL";
        hud_text(tx, ty, enemies_active_hostile(target) ? 0xFF7A60 : 0x8CE0A0, who);
        hud_text(tx, ty + 11, 0xE2E8F4, ship_looks[t->def_index].name);
        gfx_bar(tx, ty + 23, 128, 7, LAYER_HUD, (int) t->hull, td->hull_max, 230, 110, 80);
        if (ship_disabled(t)) hud_text(tx, ty + 33, 0xFFE678, "DISABLED");
        else {
            int tot = 0;
            for (int a = 0; a < ARC_COUNT; a++) tot += t->shield[a];
            gfx_bar(tx, ty + 33, 128, 5, LAYER_HUD, tot, td->shield_arc_max * ARC_COUNT, 90, 170, 255);
        }
        hud_plate(tx - 8, ty - 6, 144, 52);
    }

    /* --- Bottom strip: pilot, money, clock, hold, fuel, speed --- */
    int speed = (int) ((ship_speed(s) * FRAMES_PER_SECOND) >> FP_SHIFT);
    int by = SCREEN_H - 34;
    snprintf(line, sizeof line, "%d CR", credits);
    hud_text(24, by, 0xFFE678, line);
    snprintf(line, sizeof line, "DAY %d %02d:%02d", clock_day(), clock_hour(), clock_minute());
    hud_text(136, by, 0x96AACC, line);
    snprintf(line, sizeof line, "HOLD %d/%d", cargo_total(), s->def.cargo_cap);
    hud_text(256, by, 0xE2E8F4, line);
    snprintf(line, sizeof line, "FUEL %d/%d%s", fuel, fuel_max, scooping ? " SCOOP" : "");
    hud_text(368, by, scooping ? 0x78E6A0 : fuel * 4 < fuel_max ? 0xFF5A46 : 0xB4A0FF, line);
    snprintf(line, sizeof line, "%s%d PX/S", s->boost ? "BOOST " : "", speed);
    hud_text(SCREEN_W - 24 - (int) strlen(line) * 8, by, s->boost ? 0xFFA050 : 0x5ADCFF, line);
    hud_plate(16, by - 6, SCREEN_W - 32, 20);

    /* --- Above the strip: navigation, course, mission, and the message line --- */
    int ny = by - 20;
    if (nav.target >= 0)
        snprintf(line, sizeof line, "NAV %s %d%s", world_nav(nav.target)->name, nav_distance(&nav, s), nav.autopilot ? "  AUTOPILOT" : "");
    else
        snprintf(line, sizeof line, "NAV: SQUARE PICKS A TARGET");
    hud_text(24, ny, nav.target >= 0 ? 0x8CE0A0 : 0x687084, line);
    if (galaxy_route() >= 0) {
        snprintf(line, sizeof line, "COURSE %s", galaxy_known(galaxy_route()) ? galaxy_sys(galaxy_route())->name : "UNCHARTED");
        hud_text(SCREEN_W - 24 - (int) strlen(line) * 8, ny, 0xB4A0FF, line);
    }
    {
        const char *mh = missions_hud(s, galaxy_current());
        if (mh) hud_text(24, ny - 12, 0xFFDC78, mh);
    }
    if (nav.msg_timer > 0) {
        int w = (int) strlen(nav.msg) * 8 + 24;
        hud_centre(ny - 52, 0xFFFFFF, nav.msg);
        hud_plate((SCREEN_W - w) / 2, ny - 58, w, 20);
    }

    /* --- Warnings and the test readouts --- */
    if (gfx_prim_overflows() > 0) {
        snprintf(line, sizeof line, "PRIM POOL FULL x%d", gfx_prim_overflows());
        hud_centre(130, 0xFF5A46, line);
    }
    if (!input_connected()) hud_centre(142, 0xFF5A46, "NO PAD ON PORT 1");
#ifdef FRONTIER_DEV
    if (cheats_on && (vbl_per_frame > 1 || stress)) {
        snprintf(line, sizeof line, "VBL %d  PRIM %dK  PROJ %d  FX %d  PIR %d  DRN %d", vbl_per_frame, gfx_prim_bytes_used() >> 10,
                 proj_count(), fx_count(), enemies_pirates_alive(), drones_alive());
        hud_text(24, 124, vbl_per_frame > 1 ? 0xFFA050 : 0x687084, line);
    }
#else
    (void) vbl_per_frame;
#endif
}

/* ------------------------------------------------------------------------ */
/* Hyperspace                                                                */
/* ------------------------------------------------------------------------ */

static int pending_dest = -1;      /* the system a running jump is heading for */

/* Heading (0 = up, 1024 = right, 2048 = down) of the direction dx, dy. */
static int heading_to(int dx, int dy) {
    int ax = abs32(dx), ay = abs32(dy);
    int sum = ax + ay;
    if (sum == 0) return 0;
    int r = 1024 * ax / sum;                         /* 0 (vertical) .. 1024 (horizontal) */
    if (dy < 0) return dx >= 0 ? r : (4096 - r) & ANGLE_MASK;
    return dx >= 0 ? 2048 - r : 2048 + r;
}

/* The next system on the plotted course, or -1. */
static int next_hop(void) {
    int route = galaxy_route();
    if (route < 0 || route == galaxy_current()) return -1;
    return galaxy_next_hop(galaxy_current(), route);
}

/* Triangle with a course plotted: starts the stop-and-charge sequence. */
static void try_jump(void) {
    int dest = next_hop();
    char buf[48];
    nav.autopilot = 0;
    if (dest < 0) { nav_message(&nav, "NO COURSE: PLOT ONE ON THE MAP (SELECT)"); return; }
    if (!ship_alive(&ship) || ship_disabled(&ship) || ship.sys[SYS_ENGINE] <= 0) {
        nav_message(&nav, "HYPERDRIVE OFFLINE");
        return;
    }
    int need;
    int lock = world_jump_lock(ship.x, ship.y, &need);
    if (lock >= 0) {
        snprintf(buf, sizeof buf, "TOO CLOSE TO THE %s TO JUMP", lock == NAV_PLANET ? "PLANET" : "STATION");
        nav_message(&nav, buf);
        return;
    }
    if (!ship.warp_ok) {
        nav_message(&nav, fit_size_class(ship.def_index) == 1 ? "NO WARP DRIVE: FIT ONE AT A SHIPYARD"
                                                              : "WARP DRIVE TOO SMALL FOR THIS HULL");
        return;
    }
    int cost = galaxy_jump_fuel(galaxy_current(), dest) * ship.warp_fuel_pct / 100;
    if (fuel < cost) {
        snprintf(buf, sizeof buf, "NOT ENOUGH FUEL: NEED %d", cost);
        nav_message(&nav, buf);
        return;
    }
    pending_dest = dest;
    const StarSystem *a = galaxy_sys(galaxy_current()), *b = galaxy_sys(dest);
    nav_jump_begin(&nav, heading_to(b->map_x - a->map_x, b->map_y - a->map_y), ship.warp_charge);
    audio_sfx(SFX_JUMP);
}

/* A course has just been plotted on the galaxy map. */
static void follow_route(void) {
    if (next_hop() >= 0) nav_message(&nav, "COURSE SET: CLEAR OF PLANETS, TRIANGLE TO JUMP");
}

/* The ship has reached the white-out: move it to the destination system. */
static const char *fleet_note;       /* a squadron's news during a jump, shown on arrival */

/* Who flies in a system: its pirates, traffic and patrols, or a Veyr swarm (M17a). */
static void system_npcs(int id) {
    const StarSystem *sys = galaxy_sys(id);
    if (sys->owner == OWN_VEYR) enemies_set_system(6, 0, 0, FAC_NONE);
    else enemies_set_system(sys->pirates, sys->merchants, faction_patrols(sys->owner), faction_of_owner(sys->owner));
    enemies_set_veyr(veyr_swarm_chance(id));
}

static void arrive(int dest, int stress) {
    int from = galaxy_current();
    int hours = galaxy_jump_hours(from, dest);

    galaxy_enter(dest);
    for (int h = 0; h < hours; h++) {
        clock_advance_hour(); market_hour();
        const char *fm = fleet_hour();
        if (fm) fleet_note = fm;
        const char *vm = veyr_hour();
        if (vm) fleet_note = vm;
    }
    fleet_jumped(from, dest);                       /* your wing came with you, for free */

    const StarSystem *sys = galaxy_sys(dest);
    system_npcs(dest);
    scan_reset();

    /* Appear at the arrival point for the system we came from, still flying in
     * the direction of the jump: at top speed, or stopped if the ship has
     * retro-thrusters to brake with. */
    int ax, ay;
    world_arrival_point(from, &ax, &ay);
    ship.x = TO_FP(ax);
    ship.y = TO_FP(ay);
    ship.angle = nav_jump_heading();
    s32 exit_speed = ship.def.brakes ? ship.def.max_speed / 2 : ship.def.max_speed;
    ship.vx = (isin(ship.angle) * exit_speed) >> FP_SHIFT;
    ship.vy = -(icos(ship.angle) * exit_speed) >> FP_SHIFT;

    drones_set_mode(stress, ship.x, ship.y);
    enemies_set_mode(stress, ship.x, ship.y);
    escort_place_near(&ship);
    launch_guard = 120;

    nav.target = -1;
    char buf[48];
    int route = galaxy_route();
    if (route == dest) galaxy_set_route(-1);
    int more = galaxy_route() >= 0;
    snprintf(buf, sizeof buf, more ? "AT %s. TRIANGLE: NEXT LEG" : "ARRIVED AT %s (+%d DAYS)",
             sys->name, (hours + 12) / 24);
    nav_message(&nav, fleet_note ? fleet_note : buf);
    fleet_note = 0;
}

/* ------------------------------------------------------------------------ */
/* Starting, loading and leaving a game (M14a)                                */
/* ------------------------------------------------------------------------ */

enum { RUN_TITLE = 0, RUN_LOAD };

/* Everything back to a fresh state (a new game, or before loading one). */
static void reset_world(void) {
    proj_init();
    fx_init();
    drones_init();
    enemies_init();
    nav_init(&nav);
    dock_init();
    clock_init();
    outfit_init();
    market_init();
    galaxy_init();
    faction_init();
    missions_init();
    conquest_init();
    veyr_init();
    story_init();
    escort_init();
    lock_on = 0;
    wreck_slot = salvage_slot = -1;
    launch_guard = 0;
    attacker = -1;
    threat_timer = 0;
    pending_dest = -1;
}

/* The system's NPCs, for wherever galaxy_current() now is. */
static void enter_current_system(void) {
    galaxy_enter(galaxy_current());
    system_npcs(galaxy_current());
    scan_reset();
}

static void new_game(int p) {
    const Pilot *pl = &pilots[p];
    reset_world();
    pilot_id = p;
    play_frames = 0;                         /* game_difficulty was set by the title screen */
    strncpy(pilot_name, pl->name, PILOT_NAME_MAX - 1);
    pilot_name[PILOT_NAME_MAX - 1] = 0;
    credits = pl->credits;
    faction_set(FAC_FEDERATION, pl->ufs);
    faction_set(FAC_REBEL, pl->fsa);
    if (pl->debt) { syn_debt = pl->debt; syn_debt_due = 30; }
    if (p == PILOT_MARIGOLD) for (int i = 0; i < GALAXY_SYSTEMS; i++) galaxy_mark_known(i);   /* her star chart */
    galaxy_enter(pl->start_sys);
    enter_current_system();

    ship_init(&ship, pl->ship, TEAM_PLAYER);
    ship.y = TO_FP(260);                     /* just off the station, nose to it */
    ship.angle = 0;
    ship.assist = 0;
    outfit_sync_fuel(&ship);
    fuel = fuel_max;
    drones_set_mode(0, ship.x, ship.y);
    enemies_set_mode(0, ship.x, ship.y);
    nav_message(&nav, "WELCOME TO THE FRONTIER. START: MENU");
#ifdef FRONTIER_HOST
    if (getenv("FR_AT_STATION")) ship.x = ship.y = 0;      /* preview renderer: start in docking range */
    if (getenv("FR_CREDITS")) credits = atoi(getenv("FR_CREDITS"));
#endif
}

/* Returns SAVE_OK, or why it couldn't. On success you're docked where you saved. */
static int load_game(int slot) {
    static Ship saved;
    int dock_nav = NAV_STATION;
    reset_world();
    int r = save_read(slot, &saved, &dock_nav);
    if (r != SAVE_OK) return r;
    enter_current_system();
    ship_init(&ship, saved.def_index, TEAM_PLAYER);
    memcpy(ship.mod_hp, saved.mod_hp, sizeof ship.mod_hp);
    ship_recompute(&ship);
    ship.hull = saved.hull;
    memcpy(ship.shield, saved.shield, sizeof ship.shield);
    memcpy(ship.sys, saved.sys, sizeof ship.sys);
    memcpy(ship.msl, saved.msl, sizeof ship.msl);
    ship.ammo = ship.msl[MSL_ROCKET];
    ship.msl_sel = saved.msl_sel;
    ship.rounds = saved.rounds;
    ship.assist = saved.assist;
    outfit_sync_fuel(&ship);
    const NavObject *o = world_nav(dock_nav);
    ship.x = TO_FP(o->x);
    ship.y = TO_FP(o->y);
    drones_set_mode(0, ship.x, ship.y);
    enemies_set_mode(0, ship.x, ship.y);
    escort_place_near(&ship);
    dock_enter(&ship, dock_nav);
    return SAVE_OK;
}

static int run_game(void);

int main(void) {
    gfx_init();
    model_init();                  /* the GTE, for the 3D models */
    sprites_init();                /* ship sprites into VRAM */
    save_init();                   /* the memory card driver (before the pads: StartCARD touches them) */
    input_init();
    galaxy_init();                 /* the title's starfield needs a system */
    audio_init();                  /* the sound bank into sound RAM; music runs in the vblank (M19a) */

    const char *notice = 0;
    int open_load = 0;
    for (;;) {
        int t = title_run(notice, open_load);
        notice = 0;
        open_load = 0;
        if (t == TITLE_LOAD) {
            int r = load_game(title_load_slot);
            if (r != SAVE_OK) { notice = save_error(r); open_load = 1; continue; }
        } else {
            new_game(t);
        }
        if (run_game() == RUN_LOAD) open_load = 1;     /* LOAD GAME in flight: the title's load screen */
    }
    return 0;
}

/* One game, until the player quits to the title or loads. */
static int run_game(void) {
    Camera cam = { ship.x, ship.y };
    int frame = 0;
    int stress = 0;
    int god = 0;                 /* dev menu: no damage */
    int last_vbl = VSync(-1);
    int vbl_per_frame = 1;
    int cpu_lines = 0;
    int dock_state = DOCK_NOTHING;
    int dock_target = 0;
    int jump_next = -1;
    int last_kills = enemies_bounties();     /* bounties already paid before this run */
    int quit_to = 0;                         /* 1 + RUN_* when leaving this game */
    int was_alive = 1;
    int target = -1;

    for (;;) {
        input_update();
        int paused = galaxy_map_active() || salvage_active() || sysmenu_active() || veyr_choice_active();   /* the map stops the game */
        if (veyr_choice_active()) {                 /* the hive: a decision, the galaxy waiting on it */
            const char *vc = veyr_choice_update();
            if (vc) { nav_message(&nav, vc); system_npcs(galaxy_current()); }
        }
        if (!paused) {
            const char *vf = veyr_frame();          /* ten hours in: here they come */
            if (vf) { nav_message(&nav, vf); system_npcs(galaxy_current()); }
            if (clock_tick()) {
                market_hour();
                const char *fm = fleet_hour();
                if (fm) nav_message(&nav, fm);
                const char *vm = veyr_hour();
                if (vm) { nav_message(&nav, vm); enemies_set_veyr(veyr_swarm_chance(galaxy_current())); }
            }
            if (veyr_phase() == VP_LOST) {          /* the last human port has fallen: the end */
                veyr_game_over();
                return RUN_TITLE;
            }
            if (launch_guard > 0) launch_guard--;
        }

        if (paused) {
            if (sysmenu_active()) {
                switch (sysmenu_update()) {
                case SM_MAP:    galaxy_map_open(); break;
                case SM_LOAD:   quit_to = 1 + RUN_LOAD; break;
                case SM_QUIT:   quit_to = 1 + RUN_TITLE; break;
                case SM_ASSIST: ship.assist ^= 1; break;
                case SM_PEACE:  enemies_set_peace(!enemies_peace(), ship.x, ship.y); break;
                case SM_ATTACK: nav_message(&nav, conquest_start(&ship)); target = -1; break;
#ifdef FRONTIER_DEV
                /* Dev menu (test discs only). */
                case SM_DEV_GOD:     god ^= 1; ship.invuln = (u8) (god | stress); break;
                case SM_DEV_GANG:    enemies_dev_gang(ship.x, ship.y); break;
                case SM_DEV_PIRATE:  enemies_dev_spawn(sysmenu_dev_arg, 0, ship.x, ship.y); break;
                case SM_DEV_WRECK:   enemies_dev_spawn(sysmenu_dev_arg, 1, ship.x, ship.y);
                                     nav_message(&nav, "A WRECK IS WAITING TO BE CLAIMED"); break;
                case SM_DEV_SHIP:
                    ship_set_def(&ship, sysmenu_dev_arg);
                    outfit_sync_fuel(&ship);
                    break;
                case SM_DEV_WARP:
                    if (sysmenu_dev_arg != galaxy_current()) {
                        nav_jump_heading_set(ship.angle);
                        arrive(sysmenu_dev_arg, stress);
                        target = -1;
                    }
                    break;
                case SM_DEV_CREDITS: credits += 100000; break;
                case SM_DEV_REFILL:
                    while (ship_repair(&ship)) { }
                    fuel = fuel_max;
                    break;
                case SM_DEV_UFS: faction_set(FAC_FEDERATION, faction_rep(FAC_FEDERATION) + sysmenu_dev_arg); break;
                case SM_DEV_FSA: faction_set(FAC_REBEL, faction_rep(FAC_REBEL) + sysmenu_dev_arg); break;
                case SM_DEV_SYN: syn_set(syn_rep() + sysmenu_dev_arg); break;
                case SM_DEV_CONTRA: {
                    int room = ship.def.cargo_cap - cargo_total();
                    int n = room < 20 ? room : 20;
                    if (n > 0) cargo[COMM_NARCOTICS] += (u16) n;
                    break;
                }
                case SM_DEV_ADDSHIP: fleet_add(sysmenu_dev_arg); break;
                case SM_DEV_VEYR: {
                    const char *vm = sysmenu_dev_arg == 0 ? veyr_begin() : sysmenu_dev_arg == 1 ? veyr_spread_now()
                                   : sysmenu_dev_arg == 2 ? veyr_reveal("DEV MENU") : veyr_dev_end(sysmenu_dev_arg - 3);
                    sysmenu_say(vm ? vm : "THE HIVE IS FOUND ALREADY");
                    system_npcs(galaxy_current());
                    break;
                }
                case SM_DEV_HOURS:
                    for (int h = 0; h < 12; h++) {
                        clock_advance_hour(); market_hour();
                        const char *fm = fleet_hour();
                        if (fm) sysmenu_say(fm);
                    }
                    break;
                case SM_DEV_CAPTURE:
                    conquest_abort_quiet();
                    nav_message(&nav, conquest_capture(galaxy_current()));
                    break;
                case SM_DEV_MISSION:
                    missions_dev_finish();
                    story_dev_nowait();
                    sysmenu_say("CONTRACTS DONE, NEW STORY ORDERS READY");
                    break;
#endif
                case SM_STRESS:
                    stress ^= 1;
                    ship.invuln = (u8) (stress | god);
                    drones_set_mode(stress, ship.x, ship.y);
                    enemies_set_mode(stress, ship.x, ship.y);
                    proj_reset_peak();
                    break;
                default: break;
                }
            } else if (salvage_active()) {
                int ev = salvage_update(&ship);
                if (ev == SALV_REMOVE) { enemies_remove(salvage_slot); target = -1; }
                else if (ev == SALV_LOOTED) enemies_mark_looted(salvage_slot);
                else if (ev == SALV_DESTRUCT) {
                    enemies_remove(salvage_slot);
                    target = -1;
                    nav_message(&nav, "SELF-DESTRUCT! YOU ESCAPED, THE WRECK IS GONE");
                } else if (ev == SALV_BLAST) {          /* a Veyr hull goes up, close (M17b) */
                    const Ship *w = enemies_ship(salvage_slot);
                    int dmg = 60 + (w ? (int) ship_defs[w->def_index].hull_max / 25 : 0);
                    if (w) fx_burst(w->x, w->y, 40, 5, 50, FX_FIRE);
                    enemies_remove(salvage_slot);
                    target = -1;
                    if (!ship.invuln) ship_damage(&ship, dmg, (int) FROM_FP(ship.x), (int) FROM_FP(ship.y));
                    nav_message(&nav, "THE VEYR HULL BLEW ITSELF APART. YOU TOOK THE BLAST");
                }
            } else if (galaxy_map_active() && galaxy_map_update() == GMAP_PLOTTED) follow_route();   /* not under the hive choice */
        } else if (ship.docked) {
            /* Docked: the menu has the pad; the rest of the world carries on. */
            if (dock_update(&ship) == DOCK_EVENT_LAUNCH) {
                launch_guard = 120;
                audio_sfx(SFX_LAUNCH);
                nav_message(&nav, "LAUNCHED");
            }
            dock_state = DOCK_NOTHING;
        } else {
            if (input_pressed(PAD_SELECT) && !nav_jump_active() && !nav_jump_charging()) galaxy_map_open();
            if (input_pressed(PAD_SQUARE))   nav_cycle(&nav);
            if (input_pressed(PAD_CIRCLE))   nav_toggle_autopilot(&nav, &ship);
            if (input_pressed(PAD_L1)) {
                static const char *what[MSL_COUNT] = {
                    "MISSILE: ROCKET (STRAIGHT AHEAD, HEAVY)",
                    "MISSILE: SEEKER (HOMES ON YOUR TARGET)",
                    "MISSILE: ION (DISABLES, NO HULL DAMAGE)" };
                if (ship.loadout->secondary == WEAPON_NONE) nav_message(&nav, "THIS SHIP HAS NO MISSILE RACK");
                else nav_message(&nav, what[ship_next_missile(&ship)]);
            }
            if (input_pressed(PAD_L3)) {
                if (lock_on) {
                    lock_on = 0;
                    nav_message(&nav, "LOCK OFF");
                } else if (enemies_ship(target)) {
                    lock_on = 1;
                    nav_message(&nav, "LOCKED ON: FOLLOWING TARGET");
                } else {
                    nav_message(&nav, "NO TARGET TO LOCK (R1 SELECTS ONE)");
                }
            }
            if (input_pressed(PAD_START)) {
                sysmenu_attack_block = conquest_why_not(&ship);
                snprintf(sysmenu_attack_what, sizeof sysmenu_attack_what, "%s", conquest_target_name());
                snprintf(sysmenu_attack_owner, sizeof sysmenu_attack_owner, "%s", conquest_owner_name());
                sysmenu_open();
            }
            if (input_pressed(PAD_TRIANGLE) && nav_jump_charging()) {
                nav_jump_abort(&nav, "JUMP CANCELLED");
            } else if (input_pressed(PAD_TRIANGLE)) {
                if (ship_alive(&ship) && !nav_jump_active()) {
                    int owner_fac = faction_of_owner(galaxy_sys(galaxy_current())->owner);
                    if (dock_state == DOCK_READY && galaxy_sys(galaxy_current())->owner == OWN_VEYR) {
                        nav_message(&nav, "THE PORT IS OVERRUN. VEYR GROWTHS COVER THE DOCKS");
                    } else if (dock_state == DOCK_READY && conquest_active()) {
                        nav_message(&nav, "DOCKING DENIED: YOU ARE BESIEGING THEM");
                    } else if (dock_state == DOCK_READY && (faction_hostile(owner_fac) || (enemies_wanted() && owner_fac >= 0))) {
                        nav_message(&nav, "DOCKING DENIED: THEY ARE HOSTILE TO YOU");
                    } else if (dock_state == DOCK_READY) {
                        nav.autopilot = 0;
                        dock_enter(&ship, dock_target);
                        audio_sfx(SFX_DOCK);
                        dock_autosave(&ship, dock_target);
                    } else if (dock_state == DOCK_TOO_FAST) {
                        nav_message(&nav, "TOO FAST TO DOCK");
                    } else if (wreck_slot >= 0) {
                        if (ship_speed(&ship) > DOCK_MAX_SPEED) {
                            nav_message(&nav, "TOO FAST TO BOARD");
                        } else if (enemies_hostiles_near(ship.x, ship.y, 400) > 0) {
                            nav_message(&nav, "PIRATES NEARBY: CLEAR THEM FIRST");
                        } else {
                            const Ship *w = enemies_ship(wreck_slot);
                            salvage_slot = wreck_slot;
                            nav.autopilot = 0;
                            salvage_open(w->def_index, w->hull * 100 / w->def.hull_max, enemies_looted(wreck_slot));
                        }
                    } else if (jump_next >= 0) {
                        try_jump();
                    } else {
                        nav_message(&nav, "NOTHING TO DOCK OR BOARD HERE");
                    }
                }
            }

            /* --- Flight --- */
            ShipControls ctl;
            ship_controls_from_pad(&ctl);

            /* The afterburner burns hyperspace fuel (M16a): a unit every BOOST_FUEL_FRAMES. */
            {
                static int was_boosting;                  /* M19a: the afterburner lights */
                if (ctl.boost && !was_boosting && fuel > 0 && ship_alive(&ship)) audio_sfx(SFX_BOOST);
                was_boosting = ctl.boost;
            }
            if (ctl.boost && ship_alive(&ship)) {
                static int boost_acc, warned;
                if (fuel <= 0) {
                    ctl.boost = 0;
                    if (!warned) { nav_message(&nav, "NO FUEL: THE AFTERBURNER IS DEAD"); warned = 1; }
                } else {
                    warned = 0;
                    if (++boost_acc >= BOOST_FUEL_FRAMES) {
                        boost_acc = 0;
                        fuel--;
                        if (fuel == 3) nav_message(&nav, "FUEL LOW: THE AFTERBURNER IS EATING YOUR JUMP FUEL");
                    }
                }
            }

            /* Lock-on: turn toward the target and keep station about 250 px from it. */
            if (lock_on) {
                const Ship *lt = enemies_ship(target);
                if (!lt || !ship_alive(&ship)) {
                    lock_on = 0;
                } else {
                    int dx = (int) FROM_FP(lt->x - ship.x), dy = (int) FROM_FP(lt->y - ship.y);
                    s32 sn = isin(ship.angle), cs = icos(ship.angle);
                    int f = (dx * sn - dy * cs) >> FP_SHIFT;
                    int r = (dx * cs + dy * sn) >> FP_SHIFT;
                    int ar = r < 0 ? -r : r;
                    ctl.turn = (s8) (f < 0 ? (r >= 0 ? 1 : -1) : (ar * 8 > f ? (r >= 0 ? 1 : -1) : 0));
                    int d = (int) approx_len(dx, dy);
                    ctl.fwd = d > 300 ? FP_ONE : d < 180 ? -FP_ONE / 2 : 0;
                }
            }
            if (nav_jump_charging()) lock_on = 0;
            nav_prepare(&nav, &ship, &ctl);

            /* Hyperspace: braking and charging block thrust; the transition blocks everything. */
            int nj = nav_jump_update(&nav, &ship, &ctl);
            if (nj == NJ_FIRE) {
                fuel -= galaxy_jump_fuel(galaxy_current(), pending_dest) * ship.warp_fuel_pct / 100;
                if (fuel < 0) fuel = 0;
            } else if (nj == NJ_ARRIVE) {
                int slipped = enemies_wanted();                  /* ran from a UFS scan and got away */
                arrive(pending_dest, stress);
                if (slipped) {
                    syn_change(3);
                    nav_message(&nav, "YOU SLIPPED THE UFS. THE SYNDICATE HEARD ABOUT IT");
                }
                cam.x = ship.x;
                cam.y = ship.y;
                target = -1;
            }

            ctl.target1 = (u8) (target >= 0 ? target + 1 : 0);   /* seekers home on it */
            ctl.tgt = enemies_ship(target);                      /* every gun tracks it inside its arc */
            if (nav_jump_active()) ctl.fire1 = ctl.fire2 = 0;
            ship_update(&ship, &ctl);
            ship_fire(&ship, &ctl);
            if (ship_lost_module) {                         /* a module was shot away */
                char lost[48];
                snprintf(lost, sizeof lost, strncmp(ship_lost_module, "ALL ", 4) == 0 ? "%s" : "%s DESTROYED",
                         ship_lost_module);
                nav_message(&nav, lost);
                ship_lost_module = 0;
            }

            /* Augmented Veyr tech (M17b): the hull knits, the reactor runs hot. */
            if (ship_alive(&ship) && !ship_disabled(&ship)) {
                static int regen_acc, nerve_acc;
                int bl = mod_level[MOD_BIOREGEN], nl = mod_level[MOD_NERVELINK];
                if (bl && (int) ship.hull < (int) ship.def.hull_max && ++regen_acc >= 90 / bl) {
                    regen_acc = 0;
                    int add = (int) ship.def.hull_max / 100; if (add < 1) add = 1;
                    ship.hull += add;
                    if ((int) ship.hull > (int) ship.def.hull_max) ship.hull = ship.def.hull_max;
                }
                if (nl && ship.energy < ship.energy_max) {
                    nerve_acc += ship.energy_regen * nl;          /* +25% a level: regen x level / 4, per second */
                    if (nerve_acc >= 240) { ship.energy += nerve_acc / 240; nerve_acc %= 240; }
                    if (ship.energy > ship.energy_max) ship.energy = ship.energy_max;
                }
            }

            /* Antimatter scoop: dead space is anywhere clear of the planet and station. */
            scooping = scoop_update(ship_alive(&ship) && !nav_jump_active() && !nav_jump_charging() &&
                                    world_jump_lock(ship.x, ship.y, 0) < 0);
            dock_state = ship_alive(&ship) ? dock_check(&ship, &dock_target) : DOCK_NOTHING;
            wreck_slot = ship_alive(&ship) ? enemies_wreck_near(FROM_FP(ship.x), FROM_FP(ship.y), SALVAGE_RANGE) : -1;
            jump_next = next_hop();
        }

        if (!paused) {
            /* A destroyed ship takes its cargo with it. */
            {
                int alive = ship_alive(&ship);
                if (was_alive && !alive && cargo_total() > 0) {
                    cargo_clear();
                    missions_ship_lost();
                    nav_message(&nav, "CARGO LOST WITH THE SHIP");
                }
                was_alive = alive;
            }

            drones_update(ship.x, ship.y, stress);
            enemies_update(&ship, stress, frame);
            if (!stress) {
                const char *sm = scan_update(&ship, nav_jump_active(), ship.docked);
                if (sm) nav_message(&nav, sm);
#ifdef FRONTIER_HOST
                if (sm && getenv("FR_SCANLOG")) fprintf(stderr, "[scan] frame %d: %s\n", frame, sm);
#endif
                const char *cq = conquest_update(&ship);
                if (cq) nav_message(&nav, cq);
                if (!ship.docked) {
                    const char *tr = conquest_tribute();
                    if (tr) nav_message(&nav, tr);
                    const char *vd = tr ? 0 : veyr_day();        /* M18b: the hive's gifts (not over the pay) */
                    if (vd) nav_message(&nav, vd);
                }
                if (ship_alive(&ship) && !nav_jump_active()) {
                    const char *mm = missions_update(&ship, galaxy_current());
                    if (mm) nav_message(&nav, mm);
                    const char *dc = syn_debt_check();
                    if (dc) nav_message(&nav, dc);
                    const char *fc = fleet_contracts_check();
                    if (fc) nav_message(&nav, fc);
                    const char *sm2 = faction_take_message();
                    if (sm2) nav_message(&nav, sm2);
                }
            }
            if (threat_timer > 0 && --threat_timer == 0) escort_threat = -1;
            if (escort_threat >= 0 && !enemies_active_hostile(escort_threat)) escort_threat = -1;
            escort_update(&ship, target);
            if (escort_take_lost() > 0) nav_message(&nav, "AN ESCORT WAS DESTROYED");
            { const char *pm = escort_take_message(); if (pm) nav_message(&nav, pm); }

            /* Bounty for each pirate the player destroys: more for bigger ships
             * (not in the stress test, which respawns them). */
            {
                int k = enemies_bounties();
                if (k > last_kills && !stress) {
                    char b[40];
                    snprintf(b, sizeof b, "PIRATE DESTROYED: BOUNTY %d", k - last_kills);
                    credits += k - last_kills;
                    nav_message(&nav, b);
                }
                last_kills = k;
            }
            {
                const char *em = enemies_take_message();
                if (em) nav_message(&nav, em);
            }

            /* Targeting: R1 cycles; otherwise keep the target while it is alive
             * and near, else pick the nearest pirate. */
            if (input_pressed(PAD_R1)) {
                target = enemies_cycle(target, ship.x, ship.y, TARGET_RANGE);
            } else if (attacker >= 0 && enemies_ship(attacker) && !enemies_active_hostile(target)) {
                target = attacker;                /* shot at: target whoever did it */
                attacker = -1;
            } else {
                attacker = -1;
                const Ship *t = enemies_ship(target);
                if (!t || approx_len(FROM_FP(t->x - ship.x), FROM_FP(t->y - ship.y)) > TARGET_RANGE)
                    target = enemies_nearest_hostile(ship.x, ship.y, TARGET_RANGE);
            }
            proj_update(frame);

            collide_begin();
            if (ship_alive(&ship) && !ship.docked)
                collide_add(FROM_FP(ship.x), FROM_FP(ship.y),
                            ship_looks[ship.def_index].radius, TEAM_PLAYER, PLAYER_ID);
            drones_add_hittables();
            enemies_add_hittables();
            escort_add_hittables();
            collide_projectiles(on_hit);

            fx_update();
            if (!nav_jump_camera_hold()) camera_follow(&cam, &ship);   /* hold still as the ship leaves */
        }

#ifdef FRONTIER_HOST
        if (getenv("FR_TELEPORT") && input_pressed(PAD_L2)) {   /* preview: L2 puts you at the station, stopped */
            ship.x = ship.y = 0; ship.vx = ship.vy = 0;
        }
#endif
        /* --- Drawing --- */
        veyr_broadcast_draw();                     /* first in: on top of everything */
        if (veyr_choice_active() || sysmenu_active() || salvage_active() || galaxy_map_active() || ship.docked)
            world_draw_stars(cam.x + TO_FP(frame / 4), cam.y);     /* the glass panels look out on drifting stars */
        if (veyr_choice_active()) {
            veyr_choice_draw();                    /* the hive's fate, over the stars */
        } else if (sysmenu_active()) {
            sysmenu_draw(ship.assist, stress, enemies_peace(), god);
        } else if (salvage_active()) {
            salvage_draw(&ship);
        } else if (galaxy_map_active()) {
            galaxy_map_draw();         /* the map is the whole screen */
        } else {
            if (!ship.docked) {        /* docked: the menu is the whole screen */
                world_draw_stars(cam.x, cam.y);
                world_draw_objects(cam.x, cam.y, frame);
                missions_draw_anomaly(cam.x, cam.y, galaxy_current(), frame);
                drones_draw(cam.x, cam.y);
                enemies_draw(cam.x, cam.y, frame);
                escort_draw(cam.x, cam.y, frame);
                proj_draw(cam.x, cam.y);
                fx_draw(cam.x, cam.y);
            }
            if (ship.docked) {
                dock_draw(&ship);      /* the menu replaces the HUD */
            } else {
                ship_draw(&ship, cam.x, cam.y, frame);
                const Ship *marked = enemies_ship(target);
                if (marked) draw_target_marker(marked, cam.x, cam.y, frame);
                nav_draw_pointer(&nav, &ship, cam.x, cam.y);
                nav_jump_draw();
                if (!nav_jump_active()) radar_draw(&ship, target, frame);
                draw_hud(&ship, stress, dock_state, jump_next, target, vbl_per_frame, cpu_lines);
            }
        }

        /* Scanlines since the last VSync(0): the CPU work for this frame. */
        cpu_lines = VSync(1);

        music_context();                                     /* M19a */
        gfx_flip();   /* waits for the GPU and vertical blank, then swaps buffers */

        int now = VSync(-1);
        vbl_per_frame = now - last_vbl;
        last_vbl = now;
        if (vbl_per_frame > 0 && vbl_per_frame < 60) play_frames += (u32) vbl_per_frame;   /* real playing time */
        frame++;
        if (quit_to) return quit_to - 1;
    }
}
