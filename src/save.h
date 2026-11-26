/*
 * FRONTIER - save games on the memory card (Milestone 14a; slots in 14f).
 *
 * Four saves on the card in slot 1: an autosave (made every time you dock) and
 * three manual saves (SAVE GAME in any dock menu). Each is its own file in the
 * console's memory card manager ("FRONTIER AUTO", "FRONTIER 1".. with the
 * pilot, day and a ship icon) and takes 2 of the card's 15 blocks. Each file
 * starts with a short summary the load screen reads without loading the save.
 */
#ifndef FRONTIER_SAVE_H
#define FRONTIER_SAVE_H

#include "ship.h"

enum { SAVE_OK = 0, SAVE_NO_CARD, SAVE_NO_FILE, SAVE_BAD, SAVE_FULL, SAVE_WRITE_ERR };

#define SAVE_SLOTS 4
#define SAVE_AUTO  0                            /* slot 0 is the autosave; 1..3 are manual */

typedef struct {
    u8  used;                                   /* 0 = empty slot (the rest is meaningless) */
    u8  pilot, difficulty, sys;
    u16 day, pad;
    u32 play_frames;
    s32 credits;
    char name[24];
} SaveInfo;

void save_init(void);                           /* starts the memory card driver (call once at boot) */
int  save_write(int slot, const Ship *s, int dock_nav);   /* SAVE_* */
int  save_read(int slot, Ship *s, int *dock_nav);         /* SAVE_*; on SAVE_OK every module is restored */
void save_scan(SaveInfo info[SAVE_SLOTS]);      /* what's in each slot (reads only the summaries) */
int  save_any(void);                            /* 1 if any slot holds a save */
const char *save_error(int code);
const char *save_slot_name(int slot);           /* "AUTOSAVE", "SAVE 1".. */
int  save_last_size(void);                      /* bytes the last save actually used */
void save_play_time(char *out, int len, u32 frames);   /* "3:07" (hours:minutes) */

/* Game-wide state that lives with the save. */
extern int pilot_id;                            /* which pilot this game is (PILOT_*) */
extern int ser_version;                         /* the version being loaded (old saves lack newer fields) */
extern int game_difficulty;                     /* DIFF_*: how hard the Veyr are */
extern u32 play_frames;                         /* real playing time, in 60ths of a second */
enum { DIFF_EASY = 0, DIFF_NORMAL, DIFF_HARD, DIFF_COUNT };

#endif
