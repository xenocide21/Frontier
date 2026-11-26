/*
 * FRONTIER - the Veyr (Milestone 17a).
 *
 * After 10 hours of real playing time the Veyr come through the nebula past the
 * Outer Rim, whatever you are doing. An emergency broadcast goes out on every
 * channel. From then on they spread world by world along the hyperspace links,
 * faster on harder settings, and every human world that falls is announced.
 * The human factions sometimes win one back; you can take any of them yourself
 * (ATTACK COLONY, or a squadron), and Veyr swarms raid the systems on their border.
 *
 * The hive is hidden in the nebula. Its links open when you find it: 15 Veyr
 * kills (their nav cores), taking a world back from them, or buying the
 * coordinates from the Syndicate black market. Take VEYR PRIME and you choose:
 * burn the hive (the UFS's order: xenocide) or seize it. Either way every world
 * the Veyr hold falls back to whoever held it, and the game goes on.
 *
 * If the Veyr take the last human port, that is the end: the credits roll and
 * it is back to the title screen.
 */
#ifndef FRONTIER_VEYR_H
#define FRONTIER_VEYR_H

#include "common.h"

#define VEYR_DEADLINE ((u32) 10 * 60 * 60 * 60)   /* 10 hours of playing time, in 60ths of a second */
#define VEYR_REVEAL_KILLS 15

enum { VP_DORMANT = 0, VP_INVADING, VP_BEATEN, VP_LOST };
enum { VEYR_END_NONE = 0, VEYR_END_XENOCIDE, VEYR_END_SEIZED, VEYR_END_LIBERATION };   /* LIBERATION: burned for the FSA (M18b) */

void veyr_init(void);
int  veyr_phase(void);
int  veyr_ending(void);                        /* VEYR_END_* once the hive is taken */
const char *veyr_frame(void);                  /* once a frame: starts the invasion on time; a message or 0 */
const char *veyr_hour(void);                   /* each game hour: the spread; a message or 0 */
const char *veyr_begin(void);                  /* the invasion starts (also the dev menu) */
const char *veyr_spread_now(void);             /* dev menu: one spread step */
const char *veyr_reveal(const char *how);      /* the hive's position is known */
int  veyr_revealed(void);
void veyr_on_kill(void);                       /* you downed a Veyr ship */
const char *veyr_on_capture(int sys, int prev_owner);   /* a system you took (from conquest) */
int  veyr_swarm_chance(int sys);               /* % of raids here that are Veyr swarms */
int  veyr_kills(void);

/* The hive choice: a full-screen decision, the game paused under it. */
int  veyr_choice_active(void);
const char *veyr_choice_update(void);          /* returns the outcome message once decided */
void veyr_choice_draw(void);

/* The emergency broadcast banner, over flight or the dock. */
void veyr_broadcast_draw(void);
void veyr_banner(const char *head, const char *l1, const char *l2);   /* the same banner, for story news (M18a) */

/* Lost: the end of the line. Runs the credits (blocking) and returns. */
void veyr_game_over(void);

/* Veyr containment rigs (M17b): Syndicate black-market tech that keeps a Veyr
 * wreck from blowing itself up when you claim it. Levels 0..3. */
#define CONTAIN_MAX 3
int  veyr_contain(void);
void veyr_contain_set(int lv);
int  veyr_contain_price(int lv);               /* the price of reaching level lv */
int  veyr_contain_needs(int def);              /* the level a Veyr hull needs: 1 light, 2 Reaver, 3 the big ones */
int  veyr_claim_risk(int def);                 /* % chance claiming it sets off the self-destruct */
int  veyr_is_ship(int def);
int  veyr_hive_mine(void);                     /* you seized the hive and still hold it */

/* After the war (M18b): the hero's side and its perks.
 *   XENOCIDE   (burned for the UFS): ADMIRAL OF THE UFS, the AURORA STAR.
 *   LIBERATION (burned for the FSA): MARSHAL OF THE FSA, the LIBERTY STAR.
 *   SEIZED     (kept the hive):      the RIM CROWN; the hive grows you Veyr ships.
 * Each gets 1500 CR a day, free fuel and 10% better prices in that side's ports, and
 * the nebula's five worlds (burned ones are resettled over ten days). */
int  veyr_hero_side(void);                     /* OWN_FEDERATION / OWN_REBEL / OWN_SYNDICATE, or -1 */
int  veyr_hero_port(int sys);                  /* a port of your side: the perks apply here */
const char *veyr_title(void);                  /* "ADMIRAL OF THE UFS" etc., or 0 */
const char *veyr_medal(void);                  /* "THE AURORA STAR" etc., or 0 */
int  veyr_settled_pct(int sys);                /* a nebula world's tribute, % (resettling after a burn) */
const char *veyr_day(void);
const char *veyr_dev_end(int how);             /* dev menu: 0 burn for the UFS, 1 for the FSA, 2 seize */                    /* each new day: the hive's gift, a message or 0 */

u8  *veyr_ser(u8 *p, int load);

#endif
