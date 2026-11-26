/*
 * FRONTIER - factions and reputation (Milestone 12).
 *
 * Two factions keep a reputation with the player, -100 .. +100: the United
 * Federation of Systems (UFS, FAC_FEDERATION) and the Free Systems Alliance
 * (FSA, FAC_REBEL). Siding with one costs standing with the other (half as much).
 *   - Shooting a patrol ship, or destroying it, lowers that faction's standing.
 *   - Destroying or disabling a pirate in a system raises its owner's standing.
 * At UNFRIENDLY or worse a faction's patrols attack the player on sight and its
 * stations refuse to let him dock. Independent and Syndicate systems have no
 * reputation. Territory sizes and patrol numbers are placeholders.
 */
#ifndef FRONTIER_FACTION_H
#define FRONTIER_FACTION_H

#include "common.h"

enum { FAC_FEDERATION = 0, FAC_REBEL, FAC_COUNT, FAC_NONE = -1 };

#define REP_HOSTILE  (-25)        /* below this a faction is hostile */
#define REP_ALLY     60           /* fully trusted: the other side turns hostile (M14f) */

void faction_init(void);
int  faction_rep(int fac);
const char *faction_name(int fac);
const char *faction_standing(int fac);       /* ALLY, FRIENDLY, NEUTRAL, UNFRIENDLY, HOSTILE */
int  faction_hostile(int fac);               /* 0 for FAC_NONE */

/* The faction that owns a system (an OWN_* value from galaxy.h), or FAC_NONE. */
int  faction_of_owner(int owner);

/* Patrol ships a system of that owner normally has (placeholder). */
int  faction_patrols(int owner);

void faction_change(int fac, int delta);     /* also moves the rival, by half, the other way */

/* Outer Rim Syndicate standing (Milestone 13h): its own track, -100 .. +100,
 * not tied to UFS or FSA. Earned by smuggling (selling contraband, most in UFS
 * space), slipping UFS scans and shooting patrols; lost by killing pirates.
 * Levels open the black market's services in Syndicate systems. */
enum { SYN_UNKNOWN = 0, SYN_RECOGNIZED, SYN_TRUSTED, SYN_ESTABLISHED, SYN_INNER_CIRCLE };
#define SYN_MARKED (-25)          /* at or below: the black market will not deal */
int  syn_rep(void);
int  syn_level(void);
const char *syn_level_name(int level);
int  syn_level_needs(int level);  /* standing that level needs */
void syn_change(int delta);
int  syn_friendly(void);          /* TRUSTED or better: pirate gangs leave you alone */

/* The smuggler's hold (bought on the black market): contraband UFS scans miss. */
extern int syn_hold_level;        /* 0..3 */
int  syn_hold_units(void);
void faction_clear_negative(void);
void faction_set(int fac, int v);     /* dev menu */
void syn_set(int v);
const char *faction_take_message(void);   /* once, when choosing a side turned someone hostile */
u8  *faction_ser(u8 *p, int load);
/* A Syndicate debt (Zsa-Zsa's start): pay it at a black market before it's due. */
extern int syn_debt, syn_debt_due;      /* credits, game day */
const char *syn_debt_check(void);       /* once a game hour: collectors when it's overdue */   /* black market: UFS and FSA standing below 0 back to 0 */

#endif
