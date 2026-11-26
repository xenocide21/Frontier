/*
 * FRONTIER - the faction story lines (Milestone 18a).
 *
 * Three story lines: the UFS (Commander Iris Hale, Naval Intelligence), the
 * FSA (Major Ada Voss, FSA Command) and the Outer Rim Syndicates (Old Mother
 * Rusk, a broker). Their contacts reach you through COMMS at any port of their
 * side. Each line is nine jobs, then THE ULTIMATUM, then the Veyr war.
 *
 *  - Steps unlock one at a time; new orders come some game hours after the last
 *    job, so a line runs to about 6-8 hours of play: the Veyr come at hour 10.
 *  - Each job is a contract on your list (cargo, courier, supply, passengers,
 *    bounty, intercept, smuggling, survey; later capture and claim). It pays
 *    credits, and standing with its side; the rivals like you less each time.
 *  - The ultimatum swears you to that side for good (a warning comes first):
 *    the others turn on you and their stories end.
 *  - The Veyr arc opens when the Veyr come, for the side you swore to: fight
 *    them, find the hive, take VEYR PRIME.
 *
 * Neutral players can still find the hive (kills, a retaken world, the black
 * market): the stories are the guided way through the war.
 */
#ifndef FRONTIER_STORY_H
#define FRONTIER_STORY_H

#include "common.h"

enum { TRACK_UFS = 0, TRACK_FSA, TRACK_SYN, TRACK_COUNT };

void story_init(void);
const char *story_track_name(int t);        /* "UFS", "FSA", "SYNDICATE" */
const char *story_contact(int t);           /* who briefs you */
int  story_sworn(void);                     /* -1, or the track you swore to */

/* What COMMS shows for a track: a status line, and whether its next job is offered here. */
const char *story_status(int t, int sys);
int  story_offer_here(int t, int sys);

/* The briefing for the offered step: a title, up to 6 lines, a warning (the ultimatum), the pay. */
const char *story_offer_title(int t);
int  story_offer_lines(int t, const char **lines);   /* returns the count */
const char *story_offer_warning(int t);             /* 0 unless it is the point of no return */
int  story_offer_pay(int t);
const char *story_accept(int t, int sys, int nav, int cargo_cap);   /* a message */

/* From the contract list. */
const char *story_completed(int t);         /* a step's contract was done: rewards, the banner */
void story_failed(int t);                   /* failed or abandoned: the contact offers it again */

void story_dev_nowait(void);               /* dev menu: new orders at once */
u8  *story_ser(u8 *p, int load);

#endif
