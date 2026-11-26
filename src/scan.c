/*
 * FRONTIER - patrol scans and smuggling (Milestone 13d). See scan.h.
 */
#include <stdio.h>

#include "audio.h"
#include "enemy.h"
#include "escort.h"
#include "faction.h"
#include "galaxy.h"
#include "market.h"
#include "mission.h"
#include "outfit.h"
#include "rng.h"
#include "veyr.h"
#include "scan.h"
#include "ship.h"

#define SCAN_NOTICE     650     /* a patrol this close hails you */
#define SCAN_RANGE      320     /* the scan only runs this close */
#define SCAN_FLEE       900     /* get this far away mid-scan and you have run */
#define SCAN_FRAMES     240     /* about 4 seconds of holding still */
#define SCAN_SLOW       FP_ONE         /* "holding still": under 60 px/s (M16c: was 30) */
#define SCAN_PATIENCE   900     /* frames they wait (from the hail) for you to stop */
#define SCAN_COOLDOWN   10800   /* after a scan, about 3 minutes before the next */
#define SCAN_PASS       1800    /* a patrol that let you pass: 30 seconds before the next roll */
#define LOOK_FRAMES     420     /* an FSA patrol circles this long */

enum { SC_IDLE = 0, SC_HAIL, SC_SCAN, SC_LOOK };

static int state, slot = -1, progress, waited, cooldown, last_pct;
static int cleared;              /* scanned clean since you last docked here: they leave you be (M16c) */
static char msg[64];

void scan_reset(void) {
    state = SC_IDLE;
    slot = -1;
    cooldown = 600;              /* a breather on arrival */
    cleared = 0;
    enemies_patrol_follow(-1, 0);
}

int scan_active(void) { return state == SC_HAIL || state == SC_SCAN; }

int scan_progress_pct(void) { return state == SC_SCAN ? progress * 100 / SCAN_FRAMES : -1; }

static int is_veyr(int def) { return def >= DEF_SKITTER && def <= DEF_LEVIATHAN; }

/* A Veyr ship flying with you: the player's own, or an escort in space. */
static int veyr_present(const Ship *pl) {
    if (is_veyr(pl->def_index)) return 1;
    for (int k = 0; k < MAX_ESCORTS; k++) {
        const Ship *e = escort_ship(k);
        if (e && is_veyr(e->def_index)) return 1;
    }
    return 0;
}

static const char *finish(int next_cooldown, const char *text) {
    state = SC_IDLE;
    slot = -1;
    cooldown = next_cooldown;
    enemies_patrol_follow(-1, 0);
    return text;
}

static const char *run_away(void) {
    enemies_set_wanted(1);
    return finish(SCAN_COOLDOWN, "UFS PATROL: FAILURE TO COMPLY. WEAPONS FREE!");
}

static int dist_to(const Ship *a, const Ship *b) {
    return (int) approx_len((int) FROM_FP(b->x - a->x), (int) FROM_FP(b->y - a->y));
}

const char *scan_update(const Ship *pl, int jumping, int docked) {
    int fac = faction_of_owner(galaxy_sys(galaxy_current())->owner);

    if (!ship_alive(pl)) { if (state != SC_IDLE) finish(SCAN_COOLDOWN, 0); return 0; }

    /* The patrol we were dealing with may be gone, or the fight may have started. */
    const Ship *p = slot >= 0 ? enemies_ship(slot) : 0;
    if (state != SC_IDLE && (!p || ship_disabled(p) || enemies_wanted() || faction_hostile(fac)))
        return finish(SCAN_COOLDOWN, 0);

    switch (state) {
    case SC_IDLE: {
        if (cooldown > 0) { cooldown--; return 0; }
        if (docked) cleared = 0;                          /* new cargo aboard, maybe: fair game again */
        if (docked || jumping || cleared || enemies_wanted() || faction_hostile(fac)) return 0;
        if (fac != FAC_FEDERATION && fac != FAC_REBEL) return 0;
        if (enemies_hostiles_near(pl->x, pl->y, 700) > 0) return 0;
        int s = enemies_nearest(ROLE_PATROL, pl->x, pl->y, SCAN_NOTICE - 140 * mod_level[MOD_JAMMER]);
        if (s < 0 || !enemies_ship(s) || ship_disabled(enemies_ship(s))) return 0;
        /* Nobody scans the admiral or the marshal: their own patrols salute (M17b, M18b). */
        if (veyr_hero_side() >= 0 && faction_of_owner(veyr_hero_side()) == fac && !veyr_present(pl)) {
            cooldown = SCAN_COOLDOWN;
            audio_sfx(SFX_SCAN);
            return fac == FAC_FEDERATION ? "UFS PATROL SALUTES THE ADMIRAL" : "FSA PATROL SALUTES THE MARSHAL";
        }
        /* Not every patrol bothers: the tighter the security, the likelier a stop (M16c). */
        {
            static const u8 odds[4] = { 20, 35, 55, 70 };
            int sec = galaxy_sys(galaxy_current())->security & 3;
            if (!(fac == FAC_FEDERATION && veyr_present(pl)) && rng_range(100) >= odds[sec]) {
                cooldown = SCAN_PASS;
                return 0;
            }
        }
        slot = s;
        if (fac == FAC_FEDERATION) {
            if (veyr_present(pl)) {
                run_away();
                return "UFS PATROL: VEYR VESSEL SIGHTED. OPEN FIRE!";
            }
            if (mod_level[MOD_TRANSPONDER] && rng_range(2)) {
                slot = -1;
                cooldown = SCAN_COOLDOWN;
                return "UFS PATROL: YOUR TRANSPONDER CHECKS OUT. CARRY ON.";
            }
            state = SC_HAIL;
            progress = waited = 0;
            last_pct = -1;
            enemies_patrol_follow(slot, 1);
            audio_sfx(SFX_SCAN);
            return "UFS PATROL: HOLD POSITION FOR A CARGO SCAN";
        }
        state = SC_LOOK;
        progress = 0;
        enemies_patrol_follow(slot, 2);
        return faction_rep(FAC_REBEL) < 0 ? "FSA PATROL: WE'RE WATCHING YOU, SPACER."
                                           : "FSA PATROL LOOKS YOU OVER";
    }

    case SC_LOOK:
        if (++progress >= LOOK_FRAMES || docked || jumping)
            return finish(SCAN_COOLDOWN, docked || jumping ? 0 : "FSA PATROL MOVES ON");
        return 0;

    case SC_HAIL:
    case SC_SCAN: {
        if (docked) return finish(SCAN_COOLDOWN / 2, 0);          /* docking at their station is complying */
        if (jumping) return run_away();
        /* Pirates about: the patrol has better things to do, and so do you. */
        if (enemies_hostiles_near(pl->x, pl->y, 700) > 0) return 0;
        int d = dist_to(pl, p);
        int still = ship_speed(pl) <= SCAN_SLOW;
        /* Only you can run: a patrol that drifts off while you sit still is its own problem (M16c). */
        if (d > SCAN_FLEE && !still) return run_away();
        if (d <= SCAN_RANGE && still) {
            state = SC_SCAN;
            progress++;
        } else if (still) {
            /* holding still, waiting for them to come alongside: no warnings */
        } else {
            if (++waited >= SCAN_PATIENCE) return run_away();
            if (waited == SCAN_PATIENCE / 2) return "UFS PATROL: CUT YOUR ENGINES. LAST WARNING.";
        }
        if (state == SC_SCAN && progress < SCAN_FRAMES) {
            int pct = progress * 100 / SCAN_FRAMES / 25 * 25;
            if (pct != last_pct) {
                last_pct = pct;
                snprintf(msg, sizeof msg, "SCANNING CARGO... %d%%", pct);
                return msg;
            }
            return 0;
        }
        if (progress < SCAN_FRAMES) return 0;

        /* Done: what did they find? */
        int mc = missions_contraband();                   /* a smuggling contract's goods go in the hidden hold first */
        int hidden = syn_hold_units();
        int units = cargo_contraband() + mc;
        if (units <= hidden) {                            /* nothing, or all of it in the hidden hold */
            faction_change(FAC_FEDERATION, 1);
            cleared = 1;
            return finish(SCAN_COOLDOWN, "UFS PATROL: SCAN CLEAR. SAFE TRAVELS.");
        }
        int keep = hidden > mc ? hidden - mc : 0;
        int fine = cargo_seize_contraband(keep, &units) / 2;
        if (mc > hidden) {                                /* the contract's goods are found: it fails */
            fine += mc * 200;
            units += mc;
            missions_seize_contraband();
        }
        int paid = fine < credits ? fine : credits;
        credits -= paid;
        faction_change(FAC_FEDERATION, -6);
        snprintf(msg, sizeof msg, "CONTRABAND SEIZED: %d UNITS. FINED %d", units, paid);
        return finish(SCAN_COOLDOWN, msg);
    }
    }
    return 0;
}
