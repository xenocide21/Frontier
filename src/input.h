/*
 * FRONTIER - controller input (port 1, digital pad or DualShock).
 *
 * Uses the BIOS pad driver, which updates once per video frame. Buttons use
 * the SDK's PAD_* bit masks from <psxpad.h>.
 */
#ifndef FRONTIER_INPUT_H
#define FRONTIER_INPUT_H

#include "common.h"

void input_init(void);
void input_update(void);          /* call once per frame, before reading */

int  input_connected(void);       /* 1 if a usable pad is plugged in */
int  input_held(u16 buttons);     /* 1 if any of these buttons is down */
int  input_pressed(u16 buttons);  /* 1 only on the frame the button went down */

/* DualShock in analog mode only (the Analog button lit). Sticks read -128..127
 * with a small dead zone; right/down are positive. Digital pads read 0. */
int  input_analog(void);
int  input_stick(int stick, int axis);   /* stick: 0 left, 1 right; axis: 0 x, 1 y */

#endif
