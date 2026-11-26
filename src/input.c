#include <psxapi.h>
#include <psxpad.h>

#include "input.h"

static uint8_t pad_buff[2][34];
static u16 buttons_now  = 0;   /* bit set = button down */
static u16 buttons_prev = 0;
static int connected    = 0;
static int analog_mode  = 0;
static int sticks[2][2];


void input_init(void) {
    InitPAD(pad_buff[0], 34, pad_buff[1], 34);
    StartPAD();
    ChangeClearPAD(0);
}

void input_update(void) {
    const PADTYPE *pad = (const PADTYPE *) pad_buff[0];

    buttons_prev = buttons_now;
    buttons_now  = 0;
    connected    = 0;
    analog_mode  = 0;
    sticks[0][0] = sticks[0][1] = sticks[1][0] = sticks[1][1] = 0;

    if (pad->stat != 0)
        return;   /* no pad, or the read failed this frame */

    if (pad->type != PAD_ID_DIGITAL &&
        pad->type != PAD_ID_ANALOG_STICK &&
        pad->type != PAD_ID_ANALOG)
        return;   /* mouse, light gun, etc. are ignored */

    connected    = 1;
    buttons_now  = (u16) ~pad->btn;   /* hardware reports 0 for "down" */

    if (pad->type == PAD_ID_ANALOG) {
        analog_mode = 1;
        int v[4] = { pad->ls_x, pad->ls_y, pad->rs_x, pad->rs_y };
        for (int i = 0; i < 4; i++) {
            int d = v[i] - 128;
            if (d > -24 && d < 24) d = 0;          /* dead zone */
            sticks[i >> 1][i & 1] = d;
        }
    }
}

int input_connected(void) {
    return connected;
}

int input_held(u16 buttons) {
    return (buttons_now & buttons) != 0;
}

int input_pressed(u16 buttons) {
    return ((buttons_now & ~buttons_prev) & buttons) != 0;
}

int input_analog(void) { return analog_mode; }

int input_stick(int stick, int axis) { return sticks[stick][axis]; }
