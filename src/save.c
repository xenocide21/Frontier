/*
 * FRONTIER - save games on the memory card (Milestone 14a). See save.h.
 *
 * File layout: the 128-byte title frame the BIOS memory card manager reads
 * ("SC", icon flags, block count, Shift-JIS title, palette), one 16x16 icon
 * frame, then our data: a header (magic, version, size, checksum) and every
 * module's state in order.
 */
#include <stdio.h>
#include <psxapi.h>
#include <sys/fcntl.h>

#include "clock.h"
#include "faction.h"
#include "galaxy.h"
#include "market.h"
#include "mission.h"
#include "outfit.h"
#include "save.h"
#include "conquest.h"
#include "veyr.h"
#include "story.h"
#include "audio.h"

#define SAVE_BLOCKS  2
#define SAVE_BYTES   (SAVE_BLOCKS * 8192)
#define DATA_AT      256                         /* after the title frame and one icon frame */
#define SAVE_MAGIC   0x544E5246u                 /* "FRNT" */
#define SAVE_VERSION 13
#define SAVE_OLDEST  9      /* M17a: the galaxy grew (the nebula), older saves cannot load (M17a saves still do) */
int ser_version = SAVE_VERSION;

int pilot_id, game_difficulty = DIFF_NORMAL;
u32 play_frames;

/* One file per slot. The BIOS wants "bu00:" + a region code + a name. */
static const char *slot_file[SAVE_SLOTS] = {
    "bu00:BASLUS-99999FRNTAUTO", "bu00:BASLUS-99999FRNT1", "bu00:BASLUS-99999FRNT2", "bu00:BASLUS-99999FRNT3" };
static const char *slot_names[SAVE_SLOTS] = { "AUTOSAVE", "SAVE 1", "SAVE 2", "SAVE 3" };
const char *save_slot_name(int slot) { return slot_names[slot & 3]; }

void save_play_time(char *out, int len, u32 frames) {
    u32 m = frames / 3600;
    snprintf(out, len, "%d:%02d", (int) (m / 60), (int) (m % 60));
}

#define INFO_AT (DATA_AT + 16)                   /* the summary, right after the header */
#define BODY_AT (INFO_AT + (int) sizeof(SaveInfo))
static u8 buf[SAVE_BYTES];
static int last_size;               /* bytes the last save used (for sizing the slots) */

void save_init(void) {
    EnterCriticalSection();
    InitCARD(1);                                  /* 1: keep the controllers working */
    StartCARD();
    _bu_init();
    ExitCriticalSection();
}

/* ASCII to full-width Shift-JIS, for the title the memory card manager shows. */
static void sjis_title(u8 *out, const char *s) {
    int n = 0;
    for (; *s && n < 62; s++) {
        u8 c = (u8) *s, hi = 0x81, lo = 0x40;      /* space */
        if (c >= 'A' && c <= 'Z')      { hi = 0x82; lo = (u8) (0x60 + c - 'A'); }
        else if (c >= 'a' && c <= 'z') { hi = 0x82; lo = (u8) (0x81 + c - 'a'); }
        else if (c >= '0' && c <= '9') { hi = 0x82; lo = (u8) (0x4F + c - '0'); }
        else if (c == '-')             { lo = 0x7C; }
        else if (c == ':')             { lo = 0x46; }
        else if (c == '.')             { lo = 0x44; }
        out[n++] = hi;
        out[n++] = lo;
    }
}

/* The icon: a little orange-and-white ship on dark blue, 16x16, 4 bits a pixel. */
static void make_icon(u8 *pal, u8 *pix) {
    static const u16 colours[4] = { 0x0000, 0x2884, 0x027F, 0x7FFF };   /* BGR555: clear, navy, orange, white */
    static const char *art[16] = {
        "1111111111111111", "1111111331111111", "1111111331111111", "1111113223111111",
        "1111113223111111", "1111132222311111", "1111322222231111", "1113222332223111",
        "1132222332222311", "1322222222222231", "1322222222222231", "1113331221333111",
        "1111111221111111", "1111113223111111", "1111111111111111", "1111111111111111" };
    for (int i = 0; i < 16; i++) { u16 c = i < 4 ? colours[i] : 0; pal[i * 2] = (u8) c; pal[i * 2 + 1] = (u8) (c >> 8); }
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x += 2)
            pix[y * 8 + x / 2] = (u8) ((art[y][x] - '0') | ((art[y][x + 1] - '0') << 4));
}

static u32 checksum(const u8 *p, int n) {
    u32 h = 2166136261u;
    for (int i = 0; i < n; i++) h = (h ^ p[i]) * 16777619u;
    return h;
}

/* Every module's state, in a fixed order. load = 0 copies out, 1 copies in. */
static u8 *serialize(u8 *p, Ship *s, int *dock_nav, int load) {
    SER(p, load, pilot_id);
    SER(p, load, game_difficulty);
    SER(p, load, play_frames);
    SER(p, load, *dock_nav);
    p = outfit_ser(p, load);
    p = market_ser(p, load);
    p = faction_ser(p, load);
    p = clock_ser(p, load);
    p = galaxy_ser(p, load);
    p = missions_ser(p, load);
    p = conquest_ser(p, load);
    p = fleet_ser(p, load);
    p = veyr_ser(p, load);
    p = story_ser(p, load);
    p = audio_ser(p, load);
    /* The ship: which hull, and what state it's in (the fit itself is in the outfit). */
    SER(p, load, s->def_index);
    SER(p, load, s->hull);
    SER(p, load, s->shield);
    SER(p, load, s->sys);
    SER(p, load, s->msl);
    SER(p, load, s->msl_sel);
    SER(p, load, s->rounds);
    SER(p, load, s->mod_hp);
    SER(p, load, s->assist);
    return p;
}

int save_write(int slot, const Ship *s, int dock_nav) {
    memset(buf, 0, sizeof buf);
    buf[0] = 'S'; buf[1] = 'C';
    buf[2] = 0x11;                                /* one icon frame */
    buf[3] = SAVE_BLOCKS;
    char title[48];
    snprintf(title, sizeof title, "FRONTIER %s %s DAY %d", slot == SAVE_AUTO ? "AUTO" : slot_names[slot] + 5,
             pilot_name, clock_day());
    sjis_title(buf + 4, title);
    make_icon(buf + 0x60, buf + 0x80);

    SaveInfo info;
    memset(&info, 0, sizeof info);
    info.used = 1;
    info.pilot = (u8) pilot_id;
    info.difficulty = (u8) game_difficulty;
    info.sys = (u8) galaxy_current();
    info.day = (u16) clock_day();
    info.play_frames = play_frames;
    info.credits = credits;
    memcpy(info.name, pilot_name, sizeof info.name);
    info.name[sizeof info.name - 1] = 0;
    memcpy(buf + INFO_AT, &info, sizeof info);

    Ship copy = *s;
    ser_version = SAVE_VERSION;
    u8 *end = serialize(buf + BODY_AT, &copy, &dock_nav, 0);
    int size = (int) (end - (buf + INFO_AT));
    if (INFO_AT + size > SAVE_BYTES) return SAVE_FULL;
    u32 hdr[4] = { SAVE_MAGIC, SAVE_VERSION, (u32) size, checksum(buf + INFO_AT, size) };
    memcpy(buf + DATA_AT, hdr, sizeof hdr);
    last_size = INFO_AT + size;
#ifdef FRONTIER_HOST
    fprintf(stderr, "[save] %d bytes of %d\n", last_size, SAVE_BYTES);
#endif

    const char *name = slot_file[slot & 3];
    int fd = open(name, FWRITE);
    if (fd < 0) {
        fd = open(name, FCREATE | FNBLOCKS(SAVE_BLOCKS));
        if (fd < 0) return SAVE_NO_CARD;
        close(fd);
        fd = open(name, FWRITE);
        if (fd < 0) return SAVE_NO_CARD;
    }
    int n = write(fd, buf, SAVE_BYTES);
    close(fd);
    return n == SAVE_BYTES ? SAVE_OK : SAVE_WRITE_ERR;
}

int save_read(int slot, Ship *s, int *dock_nav) {
    int fd = open(slot_file[slot & 3], FREAD);
    if (fd < 0) return SAVE_NO_FILE;
    int n = read(fd, buf, SAVE_BYTES);
    close(fd);
    if (n != SAVE_BYTES) return SAVE_BAD;
    u32 hdr[4];
    memcpy(hdr, buf + DATA_AT, sizeof hdr);
    if (hdr[0] != SAVE_MAGIC || hdr[1] < SAVE_OLDEST || hdr[1] > SAVE_VERSION || hdr[2] > (u32) (SAVE_BYTES - INFO_AT) ||
        hdr[3] != checksum(buf + INFO_AT, (int) hdr[2]))
        return SAVE_BAD;
    ser_version = (int) hdr[1];
    serialize(buf + BODY_AT, s, dock_nav, 1);
    ser_version = SAVE_VERSION;
    return SAVE_OK;
}

/* Reads the first 384 bytes of each file: the title frame, the icon, the header
 * and the summary. Card reads go in 128-byte sectors. */
void save_scan(SaveInfo info[SAVE_SLOTS]) {
    static u8 head[384];
    for (int i = 0; i < SAVE_SLOTS; i++) {
        memset(&info[i], 0, sizeof info[i]);
        int fd = open(slot_file[i], FREAD);
        if (fd < 0) continue;
        int n = read(fd, head, sizeof head);
        close(fd);
        u32 hdr[2];
        memcpy(hdr, head + DATA_AT, sizeof hdr);
        if (n != (int) sizeof head || hdr[0] != SAVE_MAGIC || hdr[1] < SAVE_OLDEST || hdr[1] > SAVE_VERSION) continue;
        memcpy(&info[i], head + INFO_AT, sizeof info[i]);
        info[i].name[sizeof info[i].name - 1] = 0;
    }
}

int save_any(void) {
    struct DIRENTRY e;
    for (int i = 0; i < SAVE_SLOTS; i++) if (firstfile(slot_file[i], &e)) return 1;
    return 0;
}

int save_last_size(void) { return last_size; }

const char *save_error(int code) {
    switch (code) {
    case SAVE_OK:        return "GAME SAVED TO THE MEMORY CARD IN SLOT 1";
    case SAVE_NO_CARD:   return "NO CARD IN SLOT 1, OR IT LACKS 2 FREE BLOCKS";
    case SAVE_NO_FILE:   return "THAT SAVE ISN'T ON THE MEMORY CARD IN SLOT 1";
    case SAVE_BAD:       return "THAT SAVE IS DAMAGED, OR FROM AN OLDER BUILD";
    case SAVE_FULL:      return "THE MEMORY CARD NEEDS 2 FREE BLOCKS";
    default:             return "COULDN'T WRITE TO THE MEMORY CARD";
    }
}
