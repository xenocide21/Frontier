/*
 * FRONTIER - music and sound effects (Milestone 19a).
 *
 * Everything is synthesized by tools/audio/gen.py into one bank of SPU ADPCM
 * samples (audio_data.c), uploaded to sound RAM at start-up. The music is a small
 * sequencer that runs in the vertical-blank interrupt so its tempo never wobbles
 * with the frame rate: voices 0-9 play the songs (with the SPU's hall reverb),
 * voices 10-23 the sound effects.
 *
 * Volumes go 0 (off) to 10, set in OPTIONS (in game, or SOUND on the title) and
 * kept in the save.
 */
#ifndef FRONTIER_AUDIO_H
#define FRONTIER_AUDIO_H

#include "common.h"
#include "audio_data.h"

void audio_init(void);

void audio_music(int song);                 /* SONG_*, or SONG_NONE: fades out and switches */
int  audio_song(void);                      /* what is playing (or about to) */

void audio_sfx(int id);                     /* SFX_*: centred, full volume */
void audio_sfx_at(int id, int dx, int dy);  /* placed by where it happens, relative to you (pixels) */
void audio_sfx_vol(int id, int pct);        /* centred, at pct % */

void audio_listen(int x, int y);            /* where you are, in world pixels (each frame in flight) */
void audio_sfx_world(int id, int x, int y); /* a sound at a world position (pixels) */

void audio_menu(void);                      /* menu clicks for this frame's d-pad, X and O */

int  audio_music_volume(void);              /* 0..10 */
int  audio_sfx_volume(void);
void audio_set_volume(int music, int sfx);

u8  *audio_ser(u8 *p, int load);

#endif
