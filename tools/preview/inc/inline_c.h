/* Host preview: the GTE operations model.c uses, emulated in C (see host.c). */
#pragma once
#include <psxgte.h>
void h_SetRotMatrix(const MATRIX *m); void h_SetTransMatrix(const MATRIX *m);
void h_SetGeomOffset(int x, int y); void h_SetGeomScreen(int h);
void h_ldv3(const SVECTOR *a, const SVECTOR *b, const SVECTOR *c); void h_rtpt(void);
void h_ldv0(const SVECTOR *a); void h_rtps(void);
void h_stsxy3(void *a, void *b, void *c); void h_stsxy(void *a); void h_stsz3(int *a, int *b, int *c); void h_stsz(int *a);
void h_nclip(void); void h_stopz(int *p);
#define gte_SetRotMatrix(m) h_SetRotMatrix(m)
#define gte_SetTransMatrix(m) h_SetTransMatrix(m)
#define gte_SetGeomOffset(x, y) h_SetGeomOffset(x, y)
#define gte_SetGeomScreen(h) h_SetGeomScreen(h)
#define gte_ldv3(a, b, c) h_ldv3(a, b, c)
#define gte_ldv0(a) h_ldv0(a)
#define gte_rtpt() h_rtpt()
#define gte_rtps() h_rtps()
#define gte_stsxy3(a, b, c) h_stsxy3(a, b, c)
#define gte_stsxy(a) h_stsxy(a)
#define gte_stsz3(a, b, c) h_stsz3(a, b, c)
#define gte_stsz(a) h_stsz(a)
#define gte_nclip() h_nclip()
#define gte_stopz(p) h_stopz(p)
