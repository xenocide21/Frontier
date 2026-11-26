/* Host preview: a software stand-in for the PlayStation's SPU (tools/preview/host_spu.c).
 * Set FR_AUDIO=/path/out.wav to record everything the game plays. */
#pragma once
#include <stdint.h>
void hspu_upload(uint32_t addr, const void *data, uint32_t size);
void hspu_voice(int v, int voll, int volr, int pitch, uint32_t addr, uint16_t adsr1, uint16_t adsr2);
void hspu_vol(int v, int l, int r);
void hspu_keyon(uint32_t mask);
void hspu_keyoff(uint32_t mask);
void hspu_reverb(uint32_t mask, int vol);
void hspu_set_vblank(void (*f)(void));
int  hspu_vblanks(void);
void hspu_vblank_tick(void);     /* host.c: once per VSync */
