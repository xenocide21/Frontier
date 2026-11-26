/*
 * FRONTIER - low-poly 3D ships: the shipyard turntable, the station in flight
 * and the battle behind the title. Meshes come from tools/shipgen/mesh.py
 * (meshes_data.c): one per ship sprite, then the station.
 *
 * The GTE (the PlayStation's geometry coprocessor) turns and projects the
 * vertices three at a time; faces are culled, lit from their stored normals,
 * depth-sorted and drawn as flat triangles.
 */
#ifndef FRONTIER_MODEL_H
#define FRONTIER_MODEL_H

#include "common.h"

typedef struct {
    u16 nv, nf;           /* nv is a multiple of 3 */
    const s16 *v;         /* x right, y forward, z up, pad; about +-118 */
    const u16 *f;         /* per face: three vertex indices */
    const u8 *c;          /* per face: r, g, b, emissive */
    const s8 *n;          /* per face: unit normal x 127, pad */
} Mesh;

extern const Mesh ship_meshes[];
#define MESH_STATION 30

/* Where a model is and how it is turned, for model_draw_at. */
typedef struct {
    s32 x, y, z;          /* camera space: x right, y UP, z into the screen (z > 0) */
    s16 heading;          /* 0..4095 about the model's up axis; 0 = nose away from the camera */
    s16 bank;             /* roll about the model's nose axis */
    s16 climb;            /* nose up */
    s16 scale;            /* FP12: 4096 = model units are world units */
} ModelPose;

void model_init(void);    /* once at start-up (switches the GTE on) */

/* The turntable: mesh centred at (cx, cy), `radius` pixels across its longest
 * half, turned by yaw and tilted toward the viewer by pitch (0 = from the
 * side, 1024 = from straight above). */
void model_draw(int mesh, int cx, int cy, int radius, int yaw, int pitch, int z);

/* A model in a 3D scene. (cx, cy) is the screen's vanishing point, h the
 * projection distance (pixels per unit at z = h... 1 at z = h). shade 0..256 dims
 * the whole model (distance haze). Draw the NEAREST model first in a layer. */
void model_draw_at(int mesh, const ModelPose *p, int cx, int cy, int h, int z, int shade);

/* Projects a point in the same camera space; returns 0 if it is behind the camera. */
int model_project(s32 x, s32 y, s32 z, int cx, int cy, int h, int *sx, int *sy);

#endif
