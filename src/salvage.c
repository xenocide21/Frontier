#include <stdio.h>
#include <string.h>
#include <psxpad.h>

#include "audio.h"
#include "gfx.h"
#include "input.h"
#include "market.h"
#include "outfit.h"
#include "rng.h"
#include "salvage.h"
#include "veyr.h"

#define PX 16
#define PY 16
#define PW 608
#define PH 448
#define LEFT 44

enum { ITEM_CLAIM = 0, ITEM_SALVAGE, ITEM_STRIP, ITEM_LEAVE, ITEM_COUNT };

static int active, def, hull, looted_flag, sel;
static char msg[48];
static int  msg_timer;

#define RISK_CLAIM    25     /* percent chance an action sets off the self-destruct */
#define RISK_SALVAGE  20
#define RISK_STRIP    15
#define RISK_LEAVE     5

static int scrap_units(void) { return (int) ship_defs[def].price / 400 + 1; }

static int risk_trips(int pct) { return rng_range(100) < pct; }

/* A Veyr wreck (M17b): everything is riskier, claiming it needs a containment rig,
 * and when it goes up it takes a chunk of your ship with it. */
static int alien(void) { return veyr_is_ship(def); }
static int boom(void) { active = 0; return alien() ? SALV_BLAST : SALV_DESTRUCT; }
static int risk(int base) { return alien() ? base + 15 : base; }

void salvage_open(int def_index, int hull_pct, int was_looted) {
    active = 1;
    def = def_index;
    hull = hull_pct;
    looted_flag = was_looted;
    sel = 0;
    msg[0] = 0;
    msg_timer = 0;
}

int salvage_active(void) { return active; }

static void say(const char *s) {
    snprintf(msg, sizeof msg, "%s", s);
    msg_timer = 180;
}

int salvage_update(const Ship *player) {
    if (!active) return SALV_NONE;
    if (msg_timer > 0) msg_timer--;

    if (input_pressed(PAD_UP))   sel = (sel + ITEM_COUNT - 1) % ITEM_COUNT;
    if (input_pressed(PAD_DOWN)) sel = (sel + 1) % ITEM_COUNT;
    int leave = input_pressed(PAD_CIRCLE) || (input_pressed(PAD_CROSS) && sel == ITEM_LEAVE);
    if (leave) {
        active = 0;
        if (risk_trips(risk(RISK_LEAVE))) return boom();
        return SALV_CLOSE;
    }
    if (!input_pressed(PAD_CROSS)) return SALV_NONE;

    switch (sel) {
    case ITEM_CLAIM:
        if (fleet_count >= FLEET_MAX) { say("FLEET IS FULL"); return SALV_NONE; }
        if (risk_trips(alien() ? veyr_claim_risk(def) : RISK_CLAIM)) return boom();
        fleet_add(def);
        active = 0;
        audio_sfx(SFX_SCOOP);
        return SALV_REMOVE;
    case ITEM_SALVAGE: {
        int n = scrap_units();
        if ((int) player->def.cargo_cap - cargo_total() < n) {
            snprintf(msg, sizeof msg, "NEED %d FREE HOLD SPACE FOR THE SCRAP", n);
            msg_timer = 180;
            return SALV_NONE;
        }
        if (risk_trips(risk(RISK_SALVAGE))) return boom();
        cargo[COMM_SCRAP] += n;
        active = 0;
        audio_sfx(SFX_SCOOP);
        return SALV_REMOVE;
    }
    case ITEM_STRIP: {
        if (looted_flag) { say("NOTHING LEFT TO TAKE"); return SALV_NONE; }
        if (risk_trips(risk(RISK_STRIP))) return boom();
        int free_room = (int) player->def.cargo_cap - cargo_total();
        int cr = 30 + rng_range(151);
        int c = rng_range(COMM_COUNT);
        int units = 1 + rng_range((int) ship_defs[def].cargo_cap / 3 + 2);
        if (units > free_room) units = free_room;
        if (units < 0) units = 0;
        credits += cr;
        if (units > 0) {
            cargo[c] += units;
            cargo_cost[c] += 0;                 /* salvage costs nothing */
            snprintf(msg, sizeof msg, "TOOK %d CR AND %d %s", cr, units, commodities[c].name);
        } else {
            snprintf(msg, sizeof msg, "TOOK %d CR. NO ROOM IN HOLD", cr);
        }
        msg_timer = 240;
        looted_flag = 1;
        audio_sfx(SFX_SCOOP);
        return SALV_LOOTED;
    }
    default:
        return SALV_NONE;
    }
}

void salvage_draw(const Ship *player) {
    if (!active) return;
    char b[64];

    if (alien()) gfx_text_big(LEFT, PY + 18, 0, 3, 150, 230, 60, "BOARDING A VEYR HULL");
    else gfx_text_big(LEFT, PY + 18, 0, 3, 255, 170, 60, "BOARDING WRECK");
    snprintf(b, sizeof b, "%s   HULL %d%%   SYSTEMS DEAD", ship_looks[def].name, hull);
    gfx_text_big(LEFT, PY + 56, 0, 1, 160, 200, 230, b);
    snprintf(b, sizeof b, "CREDITS %d   FLEET %d/%d   HOLD %d/%d", credits, fleet_count, FLEET_MAX,
             cargo_total(), player->def.cargo_cap);
    gfx_text_big(LEFT, PY + 74, 0, 1, 255, 230, 80, b);

    static const char *names[ITEM_COUNT] = { "CLAIM SHIP FOR YOUR FLEET", "SALVAGE FOR SCRAP METAL", "STRIP CARGO AND CREDITS", "LEAVE IT" };
    for (int i = 0; i < ITEM_COUNT; i++) {
        int y = PY + 120 + i * 32;
        int on = (i == sel);
        gfx_button(LEFT, y, 420, 24, 0, on ? BTN_SELECTED : BTN_NORMAL, 2, names[i]);
    }

    const char *hint = "";
    if (sel == ITEM_CLAIM) {
        if (!alien()) hint = "KEEP THE SHIP (SWAP INTO IT AT A SHIPYARD). 25% RISK.";
        else if (veyr_claim_risk(def) >= 100) {
            snprintf(b, sizeof b, "NEEDS A LEVEL %d CONTAINMENT RIG (YOURS: %d). IT WILL BLOW.", veyr_contain_needs(def), veyr_contain());
            hint = b;
        } else { snprintf(b, sizeof b, "CONTAINMENT RIG %d HOLDS IT... MOSTLY. %d%% RISK.", veyr_contain(), veyr_claim_risk(def)); hint = b; }
    }
    if (sel == ITEM_SALVAGE) { snprintf(b, sizeof b, "BREAK IT UP: %d SCRAP METAL FOR YOUR HOLD. %d%% RISK.", scrap_units(), risk(RISK_SALVAGE)); hint = b; }
    if (sel == ITEM_STRIP) {
        if (looted_flag) hint = "ALREADY STRIPPED.";
        else { snprintf(b, sizeof b, "RANDOM CREDITS AND CARGO. ONCE PER WRECK. %d%% RISK.", risk(RISK_STRIP)); hint = b; }
    }
    if (sel == ITEM_LEAVE) { snprintf(b, sizeof b, "DRIFT AWAY. THE WRECK STAYS. %d%% RISK.", risk(RISK_LEAVE)); hint = b; }
    if (alien()) gfx_text_big(LEFT, PY + 120 + ITEM_COUNT * 32 + 26, 0, 1, 255, 120, 90,
                              "A VEYR HULL THAT GOES UP TAKES PART OF YOUR SHIP WITH IT.");
    gfx_text_big(LEFT, PY + 120 + ITEM_COUNT * 32 + 8, 0, 1, 130, 200, 150, hint);

    if (msg_timer > 0) gfx_text_big(LEFT, PY + PH - 50, 0, 1, 255, 200, 90, msg);
    gfx_text(LEFT, PY + PH - 24, 0, "UP/DOWN CHOOSE   CROSS SELECT   CIRCLE LEAVE");

    gfx_panel(PX, PY, PW, PH, 1);
}
