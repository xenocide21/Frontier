#include <stdio.h>
#include <string.h>
#include <psxpad.h>

#include "audio.h"
#include "gfx.h"
#include "input.h"
#include "outfit.h"
#include "faction.h"
#include "galaxy.h"
#include "ship.h"
#include "save.h"
#include "sysmenu.h"

#define PX 16
#define PY 16
#define PW 608
#define PH 448
#define LEFT 44
#define TOP  (PY + 70)

enum { PAGE_MAIN = 0, PAGE_FLEET, PAGE_OPTIONS, PAGE_CONTROLS, PAGE_DEV };
enum { M_RESUME = 0, M_FLEET, M_MAP, M_ATTACK, M_OPTIONS, M_CONTROLS, M_SAVE, M_LOAD, M_QUIT, M_DEV, M_COUNT };
int cheats_on;
static int main_rows(void) { return cheats_on ? M_COUNT : M_DEV; }     /* DEV MENU only with the cheat code */
enum { O_ASSIST = 0, O_MUSIC, O_SFX, O_COUNT };     /* M19a: the volumes */

/* Dev menu rows (hidden behind the title-screen cheat code, M16b). */
enum { D_GOD = 0, D_STRESS, D_PEACE, D_GANG, D_PIRATE, D_WRECK, D_SHIP, D_WARP, D_CREDITS, D_REFILL,
       D_UFS, D_FSA, D_SYN, D_CONTRA, D_MISSION, D_CAPTURE, D_ADDSHIP, D_HOURS, D_VEYR, D_COUNT };
static int dev_veyr;          /* 0 invade now, 1 spread, 2 reveal the hive */
const char *sysmenu_attack_block;
char sysmenu_attack_what[32], sysmenu_attack_owner[24];
int sysmenu_dev_arg;
static int dev_ship = DEF_RAIDER, dev_sys;

static int active, page, sel, confirm;
static u8 sq_pick[SQUADRONS];      /* the destination being chosen for each squadron (M15a) */
static u8 sq_mode[SQUADRONS];      /* SEND, TRADE or ATTACK (M15b) */
static char msg[72];
static int msg_timer;

static const char *main_names[] = { "RESUME", "FLEET", "GALAXY MAP", "ATTACK COLONY", "OPTIONS", "CONTROLS",
                                    "SAVE GAME", "LOAD GAME", "QUIT GAME", "DEV MENU" };

static int sq_can_pick(int q, int id);
void sysmenu_open(void) {
    active = 1; page = PAGE_MAIN; sel = 0; msg_timer = 0; confirm = 0;
    for (int q = 1; q < SQUADRONS; q++) if (!sq_can_pick(q, sq_pick[q])) sq_pick[q] = (u8) galaxy_current();
}
int  sysmenu_active(void) { return active; }

static void say(const char *s) { snprintf(msg, sizeof msg, "%s", s); msg_timer = 180; }

static int rows(void) {
    if (page == PAGE_MAIN)    return main_rows();
    if (page == PAGE_OPTIONS) return O_COUNT;
    if (page == PAGE_DEV)     return D_COUNT;
    if (page == PAGE_FLEET)   return fleet_count > 0 ? fleet_count + SQUADRONS - 1 : 1;
    return 1;
}

static int dev_update(void) {
    int lr = input_pressed(PAD_RIGHT) ? 1 : input_pressed(PAD_LEFT) ? -1 : 0;
    if (lr && (sel == D_PIRATE || sel == D_WRECK || sel == D_SHIP || sel == D_ADDSHIP))
        dev_ship = (dev_ship + SHIP_DEF_COUNT + lr) % SHIP_DEF_COUNT;
    if (lr && sel == D_VEYR) dev_veyr = (dev_veyr + 6 + lr) % 6;
    if (lr && sel == D_WARP) dev_sys = (dev_sys + GALAXY_SYSTEMS + lr) % GALAXY_SYSTEMS;
    if (lr && (sel == D_UFS || sel == D_FSA || sel == D_SYN)) {      /* standings move at once */
        sysmenu_dev_arg = lr * 10;
        return sel == D_UFS ? SM_DEV_UFS : sel == D_FSA ? SM_DEV_FSA : SM_DEV_SYN;
    }
    if (!input_pressed(PAD_CROSS)) return SM_NONE;
    switch (sel) {
    case D_GOD:     return SM_DEV_GOD;
    case D_STRESS:  active = 0; return SM_STRESS;
    case D_PEACE:   return SM_PEACE;
    case D_GANG:    active = 0; return SM_DEV_GANG;
    case D_PIRATE:  active = 0; sysmenu_dev_arg = dev_ship; return SM_DEV_PIRATE;
    case D_WRECK:   active = 0; sysmenu_dev_arg = dev_ship; return SM_DEV_WRECK;
    case D_SHIP:    sysmenu_dev_arg = dev_ship; say("NOW FLYING IT (DEFAULT FIT)"); return SM_DEV_SHIP;
    case D_WARP:    active = 0; sysmenu_dev_arg = dev_sys; return SM_DEV_WARP;
    case D_CREDITS: say("+100000 CREDITS"); return SM_DEV_CREDITS;
    case D_REFILL:  say("REPAIRED, REARMED AND REFUELLED"); return SM_DEV_REFILL;
    case D_CONTRA:  say("LOADED CONTRABAND (AS MUCH AS FITS, UP TO 20)"); return SM_DEV_CONTRA;
    case D_MISSION: return SM_DEV_MISSION;
    case D_CAPTURE: active = 0; return SM_DEV_CAPTURE;
    case D_ADDSHIP: sysmenu_dev_arg = dev_ship; say("ADDED TO YOUR FLEET (YOUR WING)"); return SM_DEV_ADDSHIP;
    case D_HOURS:   say("12 GAME HOURS PASSED"); return SM_DEV_HOURS;
    case D_VEYR:    sysmenu_dev_arg = dev_veyr; return SM_DEV_VEYR;
    default:        return SM_NONE;
    }
}

void sysmenu_say(const char *s2) { say(s2); }

/* ------------------------------------------------------------------------ */
/* The FLEET page (M15a): every ship, where it is and what it belongs to,    */
/* then squadrons A, B and C with their orders.                              */
/* ------------------------------------------------------------------------ */


static void fleet_ship_row(int i) {
    int here = fleet_here(i), q = fleet_sq[i];
    int lr = input_pressed(PAD_RIGHT | PAD_CROSS) ? 1 : input_pressed(PAD_LEFT) ? STANCE_COUNT - 1 : 0;
    if (lr) {
        if (q != 0) { say("SQUARE PUTS IT IN YOUR WING FIRST"); return; }
        int ns = (fleet_stance[i] + lr) % STANCE_COUNT;
        if (!here && ns != STANCE_HANGAR) {
            char t[64];
            snprintf(t, sizeof t, "IT IS DOCKED AT %s: FLY THERE TO PICK IT UP", galaxy_sys(fleet_sys[i])->name);
            say(t);
            return;
        }
        fleet_stance[i] = (u8) ns;
    }
    if (input_pressed(PAD_SQUARE)) {
        if (q && sq_order[q] == SQ_MOVE) { say("IT IS IN HYPERSPACE WITH ITS SQUADRON"); return; }
        for (int k = 1; k < SQUADRONS; k++) {           /* the next group this ship can join */
            int nq = (q + k) % SQUADRONS, w = sq_where(nq);
            if (nq == 0 ? !here : (w >= 0 && (w != fleet_sys[i] || sq_order[nq] == SQ_MOVE))) continue;
            fleet_sq[i] = (u8) nq;
            if (nq == 0) { fleet_stance[i] = STANCE_FOLLOW; say("IT JOINS YOUR WING: IT JUMPS WITH YOU, ON YOUR FUEL"); }
            else if (sq_count(nq) == 1) sq_order[nq] = SQ_HOLD;     /* a new squadron holds here */
            if (q && sq_count(q) == 0) sq_order[q] = SQ_HOLD;
            return;
        }
        say(here ? "NO SQUADRON IT CAN JOIN HERE" : "ONLY A SQUADRON IN ITS SYSTEM CAN TAKE IT");
    }
}

/* A squadron can go anywhere charted, or blind to a system linked to where it is. */
static int sq_can_pick(int q, int id) {
    int w = sq_where(q);
    return galaxy_known(id) || (w >= 0 && galaxy_link_index(w, id) >= 0);
}

/* A destination's name; an uncharted one by its bearing from the squadron. */
static const char *pick_name(int q, int id) {
    static char t[24];
    if (galaxy_known(id)) return galaxy_sys(id)->name;
    int w = sq_where(q); if (w < 0) w = galaxy_current();
    int dx = galaxy_sys(id)->map_x - galaxy_sys(w)->map_x, dy = galaxy_sys(id)->map_y - galaxy_sys(w)->map_y;
    int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    snprintf(t, sizeof t, "UNCHARTED %s%s", ay * 2 > ax ? (dy < 0 ? "N" : "S") : "", ax * 2 > ay ? (dx < 0 ? "W" : "E") : "");
    return t;
}

static void fleet_sq_row(int q) {
    int lr = input_pressed(PAD_RIGHT) ? 1 : input_pressed(PAD_LEFT) ? -1 : 0;
    for (int k = 0; lr && k < GALAXY_SYSTEMS; k++) {
        sq_pick[q] = (u8) ((sq_pick[q] + GALAXY_SYSTEMS + lr) % GALAXY_SYSTEMS);
        if (sq_can_pick(q, sq_pick[q])) break;
    }
    if (input_pressed(PAD_TRIANGLE)) sq_mode[q] = (u8) ((sq_mode[q] + 1) % MISSION_COUNT);
    if (input_pressed(PAD_CROSS)) say(sq_order_ship(q, sq_pick[q], sq_mode[q]));
}

static void fleet_page_update(void) {
    if (fleet_count == 0) return;
    if (sel < fleet_count) fleet_ship_row(sel);
    else                   fleet_sq_row(sel - fleet_count + 1);
}

int sysmenu_update(void) {
    if (!active) return SM_NONE;
    audio_menu();
    if (msg_timer > 0) msg_timer--;

    int n = rows();
    if (input_pressed(PAD_UP))   sel = (sel + n - 1) % n;
    if (input_pressed(PAD_DOWN)) sel = (sel + 1) % n;

    if (input_pressed(PAD_START)) {
        if (page == PAGE_MAIN) active = 0; else { page = PAGE_MAIN; sel = 0; }
        return SM_NONE;
    }
    if (input_pressed(PAD_CIRCLE)) {
        if (page == PAGE_MAIN) active = 0; else { page = PAGE_MAIN; sel = 0; }
        return SM_NONE;
    }

    if (page == PAGE_FLEET) {
        fleet_page_update();
        return SM_NONE;
    }

    if (page == PAGE_DEV) return dev_update();

    if (page == PAGE_OPTIONS && sel != O_ASSIST) {        /* M19a: LEFT and RIGHT set a volume */
        int lr = input_pressed(PAD_RIGHT) ? 1 : input_pressed(PAD_LEFT) ? -1 : 0;
        if (lr) {
            int m = audio_music_volume(), s = audio_sfx_volume();
            if (sel == O_MUSIC) m += lr; else s += lr;
            audio_set_volume(m, s);
            if (sel == O_SFX) audio_sfx(SFX_CASH);         /* a sample at the new level */
        }
        return SM_NONE;
    }

    if (!input_pressed(PAD_CROSS)) return SM_NONE;

    if (page == PAGE_OPTIONS) {
        active = 0;
        return SM_ASSIST;
    }
    if (page == PAGE_MAIN) {
        if (confirm && confirm != sel) confirm = 0;
        switch (sel) {
        case M_RESUME:   active = 0; break;
        case M_FLEET:    page = PAGE_FLEET; sel = 0; break;
        case M_MAP:      active = 0; return SM_MAP;
        case M_OPTIONS:  page = PAGE_OPTIONS; sel = 0; break;
        case M_CONTROLS: page = PAGE_CONTROLS; sel = 0; break;
        case M_DEV:      page = PAGE_DEV; sel = 0; break;
        case M_ATTACK:
            if (sysmenu_attack_block) { say(sysmenu_attack_block); break; }
            if (!confirm) {
                char w[64];
                snprintf(w, sizeof w, "X AGAIN: ATTACK %s (%s TURN HOSTILE)", sysmenu_attack_what, sysmenu_attack_owner);
                confirm = M_ATTACK; say(w); break;
            }
            active = 0; return SM_ATTACK;
        case M_SAVE:     say("SAVE AT ANY DOCK: SAVE GAME IN THE DOCK MENU"); break;
        case M_LOAD:
            if (!save_any()) { say("NO FRONTIER SAVES ON THE CARD IN SLOT 1"); break; }
            if (!confirm) { confirm = M_LOAD; say("X AGAIN TO PICK A SAVE (UNSAVED PROGRESS IS LOST)"); break; }
            active = 0; return SM_LOAD;
        case M_QUIT:
            if (!confirm) { confirm = M_QUIT; say("X AGAIN TO QUIT (UNSAVED PROGRESS IS LOST)"); break; }
            active = 0; return SM_QUIT;
        default:         break;
        }
    }
    return SM_NONE;
}

static void t(int y, int r, int g, int b, const char *s) { gfx_text_big(LEFT, y, 0, 1, r, g, b, s); }

static void dev_draw(int stress, int peace, int god) {
    char b[72];
    for (int i = 0; i < D_COUNT; i++) {
        switch (i) {
        case D_GOD:     snprintf(b, sizeof b, "GOD MODE  %s", god ? "ON" : "OFF"); break;
        case D_STRESS:  snprintf(b, sizeof b, "STRESS TEST  %s", stress ? "ON" : "OFF"); break;
        case D_PEACE:   snprintf(b, sizeof b, "PEACE MODE (NO PIRATES)  %s", peace ? "ON" : "OFF"); break;
        case D_GANG:    snprintf(b, sizeof b, "SPAWN A PIRATE GANG NOW"); break;
        case D_PIRATE:  snprintf(b, sizeof b, "SPAWN HOSTILE  < %s >", ship_looks[dev_ship].name); break;
        case D_WRECK:   snprintf(b, sizeof b, "SPAWN WRECK TO CLAIM  < %s >", ship_looks[dev_ship].name); break;
        case D_SHIP:    snprintf(b, sizeof b, "FLY SHIP  < %s >", ship_looks[dev_ship].name); break;
        case D_WARP:    snprintf(b, sizeof b, "WARP TO  < %s >", galaxy_sys(dev_sys)->name); break;
        case D_CREDITS: snprintf(b, sizeof b, "+100000 CREDITS"); break;
        case D_REFILL:  snprintf(b, sizeof b, "REPAIR, REARM, REFUEL"); break;
        case D_UFS:     snprintf(b, sizeof b, "UFS STANDING  < %d >", faction_rep(FAC_FEDERATION)); break;
        case D_FSA:     snprintf(b, sizeof b, "FSA STANDING  < %d >", faction_rep(FAC_REBEL)); break;
        case D_SYN:     snprintf(b, sizeof b, "SYNDICATE STANDING  < %d >", syn_rep()); break;
        case D_CONTRA:  snprintf(b, sizeof b, "LOAD 20 CONTRABAND (TEST SCANS)"); break;
        case D_MISSION: snprintf(b, sizeof b, "COMPLETE CONTRACTS, STORY ORDERS NOW"); break;
        case D_CAPTURE: snprintf(b, sizeof b, "CAPTURE THIS SYSTEM (NO FIGHT)"); break;
        case D_ADDSHIP: snprintf(b, sizeof b, "ADD TO FLEET  < %s >", ship_looks[dev_ship].name); break;
        case D_HOURS:   snprintf(b, sizeof b, "PASS 12 GAME HOURS (SQUADRONS TRAVEL)"); break;
        default: {
            static const char *va[6] = { "INVADE NOW", "SPREAD ONE STEP", "REVEAL THE HIVE", "END: BURN FOR UFS", "END: BURN FOR FSA", "END: SEIZE HIVE" };
            snprintf(b, sizeof b, "VEYR  < %s >", va[dev_veyr]);
            break;
        }
        }
        gfx_button(LEFT, TOP - 14 + i * 17, 520, 15, 0, i == sel ? BTN_SELECTED : BTN_NORMAL, 1, b);
    }
    t(TOP - 14 + D_COUNT * 17 + 4, 150, 170, 200, "LEFT RIGHT: CHANGE VALUE   CROSS: DO IT   O: BACK");
}

void sysmenu_draw(int assist, int stress, int peace, int god) {
    if (!active) return;
    char b[72];
    static const char *titles[5] = { "SYSTEM MENU", "FLEET", "OPTIONS", "CONTROLS", "DEV MENU" };
    gfx_text_big(LEFT, PY + 18, 0, 3, 90, 220, 255, titles[page]);

    if (page == PAGE_MAIN) {
        for (int i = 0; i < main_rows(); i++) {
            int dis = i == M_SAVE || (i == M_ATTACK && sysmenu_attack_block);
            gfx_button(LEFT, TOP + i * 30, 300, 26, 0, i == sel ? BTN_SELECTED : dis ? BTN_DISABLED : BTN_NORMAL, 2, main_names[i]);
        }
    } else if (page == PAGE_OPTIONS) {
        for (int i = 0; i < O_COUNT; i++) {
            if (i == O_ASSIST) snprintf(b, sizeof b, "FLIGHT ASSIST  %s", assist ? "ON" : "OFF");
            else {
                int v = i == O_MUSIC ? audio_music_volume() : audio_sfx_volume();
                snprintf(b, sizeof b, "%s  < %2d >%s", i == O_MUSIC ? "MUSIC  " : "EFFECTS", v, v ? "" : "   OFF");
                if (v) gfx_bar(LEFT + 340, TOP + i * 34 + 9, 160, 9, 0, v, 10, 255, 205, 80);   /* on the button */
            }
            gfx_button(LEFT, TOP + i * 34, 520, 26, 0, i == sel ? BTN_SELECTED : BTN_NORMAL, 2, b);
        }
        t(TOP + O_COUNT * 34 + 6, 150, 170, 200, "LEFT RIGHT: VOLUME   CROSS: FLIGHT ASSIST (CLOSES THE MENU)");
        t(TOP + O_COUNT * 34 + 20, 150, 170, 200, "CIRCLE: BACK. THE VOLUMES ARE KEPT IN YOUR SAVE.");
    } else if (page == PAGE_DEV) {
        dev_draw(stress, peace, god);
    } else if (page == PAGE_FLEET) {
        if (fleet_count == 0) {
            gfx_button(LEFT, TOP, 520, 26, 0, BTN_DISABLED, 2, "NO CLAIMED SHIPS YET");
            t(TOP + 40, 150, 170, 200, "DISABLE A SHIP AND BOARD IT TO CLAIM IT. CIRCLE: BACK");
        } else {
            static const char *sl[SQUADRONS] = { "", "SQ A", "SQ B", "SQ C" };
            int y = TOP - 14;
            for (int i = 0; i < fleet_count; i++, y += 20) {
                const char *where = fleet_here(i) ? "HERE" : galaxy_sys(fleet_sys[i])->name;
                char who[24];
                snprintf(who, sizeof who, "%s %s", rank_short(fleet_rank(i)), fleet_pname[i]);
                if (fleet_sq[i] == 0)
                    snprintf(b, sizeof b, "%-10s %-15s WING %-7s %-8s F%d", ship_looks[fleet[i]].name, who,
                             stance_name(fleet_stance[i]), where, fleet_fuel[i]);
                else if (sq_order[fleet_sq[i]] == SQ_MOVE)
                    snprintf(b, sizeof b, "%-10s %-15s %s IN HYPERSPACE    F%d", ship_looks[fleet[i]].name, who,
                             sl[fleet_sq[i]], fleet_fuel[i]);
                else
                    snprintf(b, sizeof b, "%-10s %-15s %s         %-8s F%d", ship_looks[fleet[i]].name, who,
                             sl[fleet_sq[i]], where, fleet_fuel[i]);
                gfx_button(LEFT, y, 560, 18, 0, i == sel ? BTN_SELECTED : BTN_NORMAL, 1, b);
            }
            y += 6;
            for (int q = 1; q < SQUADRONS; q++, y += 20) {
                int n = sq_count(q), w = sq_where(q);
                char st[40];
                if (n == 0) snprintf(st, sizeof st, "NO SHIPS");
                else if (sq_order[q] == SQ_MOVE)
                    snprintf(st, sizeof st, "TO %s, %dH", pick_name(q, sq_next[q]), sq_eta[q]);
                else snprintf(st, sizeof st, "%s AT %s", sq_order[q] == SQ_STRANDED ? "STRANDED" : "HOLDS", galaxy_sys(w)->name);
                if (n && sq_mission[q] != MISSION_GO && sq_order[q] != SQ_STRANDED)     /* what it's doing */
                    snprintf(st + strlen(st), sizeof st - strlen(st), sq_mission[q] == MISSION_TRADE ? " TRD" : " ATK");
                snprintf(b, sizeof b, "%s %d: %-23s %s < %s >", sq_name(q), n, st, mission_name(sq_mode[q]),
                         pick_name(q, sq_pick[q]));
                int row = fleet_count + q - 1;
                gfx_button(LEFT, y, 560, 18, 0, row == sel ? BTN_SELECTED : n ? BTN_NORMAL : BTN_DISABLED, 1, b);
            }
            y += 6;
            if (sel < fleet_count) {
                t(y, 150, 170, 200, "LEFT/RIGHT: STANCE (WING)   SQUARE: WING / SQUADRON A / B / C");
                t(y + 14, 130, 200, 150, "HANGAR: WAITS DOCKED  FOLLOW: NO FIGHTS  GUARD: DEFENDS YOU  ATTACK");
                t(y + 28, 130, 200, 150, "YOUR WING JUMPS WITH YOU FOR FREE: ONLY YOUR SHIP BURNS FUEL");
            } else {
                t(y, 150, 170, 200, "LEFT/RIGHT: WHERE   TRIANGLE: SEND / TRADE / ATTACK   CROSS: GO");
                t(y + 14, 130, 200, 150, "SEND: GO AND HOLD (GUARD) IT. TRADE: CARRY CARGO BACK AND FORTH");
                t(y + 28, 130, 200, 150, "ATTACK: FIGHT ITS WHOLE DEFENCE. TOO FEW SHIPS AND THEY DIE");
                t(y + 42, 130, 200, 150, "ITS LEAD SHIP BURNS THE FUEL. STRANDED: BRING ITS SHIPS INTO YOUR WING");
            }
        }
    } else {
        static const char *lines[] = {
            "D-PAD OR LEFT STICK  TURN AND THRUST",
            "RIGHT STICK          STRAFE (ANALOG MODE ONLY)",
            "CROSS                AFTERBURNER (HOLD)",
            "R2                   GUNS (AIM AT TARGET IN ARC)",
            "L2                   FIRE THE SELECTED MISSILE",
            "L1                   MISSILE: ROCKET, SEEKER, ION",
            "R1                   CHANGE TARGET SHIP",
            "SQUARE               CYCLE NAV: STATION, PLANET",
            "TRIANGLE             DOCK, BOARD, OR JUMP ON COURSE",
            "CIRCLE               AUTOPILOT ON/OFF",
            "LEFT STICK CLICK     LOCK ON TO YOUR TARGET AND FOLLOW IT",
            "SELECT               GALAXY MAP",
            "START                THIS MENU",
        };
        for (int i = 0; i < 13; i++) t(TOP + i * 14, 210, 220, 235, lines[i]);
        snprintf(b, sizeof b, "CONTROLLER: %s", input_analog() ? "ANALOG MODE (STICKS ON)" : "DIGITAL (PRESS THE ANALOG BUTTON)");
        t(TOP + 13 * 14 + 10, input_analog() ? 120 : 255, input_analog() ? 220 : 200, input_analog() ? 160 : 90, b);
    }

    if (msg_timer > 0) t(PY + PH - 50, 255, 200, 90, msg);
    t(PY + PH - 26, 150, 160, 180, page == PAGE_MAIN ? "UP DOWN: CHOOSE   X: SELECT   O OR START: RESUME"
                                                     : "O OR START: BACK");
    gfx_panel(PX, PY, PW, PH, 1);
}
