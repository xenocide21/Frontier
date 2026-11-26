/*
 * FRONTIER - NPC ships: pirates and merchants.
 *
 * They use the same Ship as the player, so shields, subsystems, speed and
 * weapons all behave identically. Only the pilot differs: a small state
 * machine fills in ShipControls each frame.
 *
 * Pirates (slots 0..5, armed, orange):
 *   IDLE    nothing in range: drift.
 *   ATTACK  pick the nearest target (the player is preferred), close in to
 *           about 200 px, lead the shot and fire when lined up.
 *   EVADE   too close: break away at full thrust for about a second.
 *   FLEE    hull below 40%: run away until out of range; a fresh pirate then
 *           takes its place.
 *   Disabled pirates (hull below 15%) drift helplessly. Shoot them again and
 *   they are destroyed.
 *
 * Merchants (slots 6..7, unarmed Hauler type, green): travel between random
 * waypoints, and run away at full thrust from any active pirate within 400 px.
 *
 * Patrols (slots 8..11, UFS blue or FSA magenta, armed): fly around, hunt
 * pirates, and attack the player too if that faction is hostile to him.
 *
 * Destroyed NPCs return later, fully repaired, at a distance.
 */
#ifndef FRONTIER_ENEMY_H
#define FRONTIER_ENEMY_H

#include "common.h"
#include "ship.h"

#define MAX_PIRATES          6
#define MAX_MERCHANTS        2
#define MAX_PATROLS          4
#define MAX_ENEMIES          (MAX_PIRATES + MAX_MERCHANTS + MAX_PATROLS)
#define PIRATE_NORMAL_COUNT  3

enum { ROLE_PIRATE = 0, ROLE_MERCHANT, ROLE_PATROL, ROLE_DEFENDER };   /* DEFENDER: a colony's defence wave (M14g) */

void enemies_init(void);
void enemies_set_mode(int stress, s32 player_x, s32 player_y);
void enemies_update(const Ship *player, int stress, int frame);

/* How many pirates and merchants a system normally has. Takes effect at the next
 * enemies_set_mode call (which also places them around the player). */
void enemies_set_system(int pirates, int merchants, int patrols, int faction);   /* faction: FAC_* or -1 */

/* Peace mode: no pirates at all (a test aid for the autopilot and navigation). */
void enemies_set_peace(int peace, s32 player_x, s32 player_y);
int  enemies_peace(void);

/* Adds every living NPC to the collision grid. */
void enemies_add_hittables(void);
void enemies_damage(int slot, int damage, int hit_x, int hit_y);
void enemies_draw(s32 cam_x, s32 cam_y, int frame);

/* Looking at NPCs (for targeting). A slot is 0..MAX_ENEMIES-1. */
const Ship *enemies_ship(int slot);      /* NULL if the slot is empty or destroyed */
int  enemies_role(int slot);
int  enemies_nearest(int role, s32 px, s32 py, int range);   /* slot, or -1 */
int  enemies_nearest_hostile(s32 px, s32 py, int range);     /* nearest ship that would fight you, or -1 */
int  enemies_cycle(int current, s32 px, s32 py, int range);  /* next slot in range, or -1 */
int  enemies_hostiles_near(s32 px, s32 py, int range);      /* active (not disabled) pirates in range */
/* Nearest ship your turrets should shoot: an active pirate or a hostile patrol, never a
 * disabled ship (so turrets never wreck a prize) or a merchant. -1 if none in range. */
int  enemies_turret_target(s32 px, s32 py, int range);
/* 1 if the slot holds an active (not disabled) pirate or hostile patrol. */
int  enemies_active_hostile(int slot);

/* Salvage (Milestone 10): the nearest disabled ship within range, or -1. */
int  enemies_wreck_near(s32 px, s32 py, int range);
int  enemies_looted(int slot);
void enemies_mark_looted(int slot);
void enemies_remove(int slot);       /* claimed or sold: the NPC is gone (a new one comes later) */

/* Patrol scans (see scan.c). */
void enemies_set_wanted(int w);          /* this system's patrols treat the player as hostile */
int  enemies_wanted(void);
void enemies_patrol_follow(int slot, int mode);   /* slot -1: none; mode 1 hold beside, 2 circle */

/* Pirate gangs (M13e). */
const char *enemies_take_message(void);  /* "PIRATE GANG ON SCANNERS..." etc., once; or 0 */
int  enemies_bounties(void);

/* Dev menu: a gang now; one pirate (hostile, or a disabled wreck to claim) near the player. */
void enemies_dev_gang(s32 px, s32 py);

/* Missions (M13j). */
int  enemies_player_pirate_kills(void);   /* pirates the player destroyed or disabled, ever */
int  enemies_take_bounty_hit(void);       /* 1 once when the wanted pilot is downed */
int  enemies_bounty_slot(void);           /* the wanted pilot's slot, or -1 */
int  enemies_is_bounty(int slot);
void enemies_spawn_bounty(int def, int wingmen, s32 px, s32 py);
void enemies_dev_spawn(int def, int wreck, s32 px, s32 py);             /* credits earned so far for pirates the player destroyed */

/* Planet conquest (M14g). A siege clears the merchant and patrol slots and holds
 * no new pirate gangs; each wave launches from (bx, by) and comes for the player.
 * Defenders never flee, never respawn and are beaten when destroyed or disabled.
 * enemies_set_mode (a jump, a reload) ends the siege. */
void enemies_siege_wave(const u8 *defs, int n, s32 bx, s32 by, int team, s32 px, s32 py);
int  enemies_defenders_left(void);       /* defenders still fighting */
int  enemies_siege(void);                /* 1 while a siege is on */
void enemies_siege_end(void);            /* traffic comes back later */

/* The Veyr (M17a): this % of raids here are Veyr swarms (100 in a Veyr-held system).
 * A swarm is hostile to everyone, ignores Syndicate standing and pays a UFS bounty. */
void enemies_set_veyr(int pct);
int  enemies_is_alien(int slot);

int enemies_pirates_alive(void);
int enemies_pirates_target(void);
int enemies_merchants_alive(void);
int enemies_kills(void);       /* pirates destroyed */
int enemies_disables(void);    /* pirates disabled */
int enemies_merchant_losses(void);

#endif
