/* Host preview: the PlayStation kernel calls the game uses. The memory card is a
 * folder (FR_CARD, default /tmp/frontier_card): each save file is a file there. */
#pragma once
static inline int EnterCriticalSection(void) { return 0; }
static inline void ExitCriticalSection(void) {}
static inline void InitCARD(int a) { (void) a; }
static inline void StartCARD(void) {}
static inline void _bu_init(void) {}
struct DIRENTRY { char name[20]; int attr, size; struct DIRENTRY *next; int head; char system[4]; };
int host_card_open(const char *name, int mode);
int host_card_read(int fd, void *buf, int n);
int host_card_write(int fd, const void *buf, int n);
void host_card_close(int fd);
struct DIRENTRY *host_card_first(const char *name, struct DIRENTRY *e);
#define open(n, m) host_card_open(n, m)
#define read(f, b, n) host_card_read(f, b, n)
#define write(f, b, n) host_card_write(f, b, n)
#define close(f) host_card_close(f)
#define firstfile(n, e) host_card_first(n, e)
