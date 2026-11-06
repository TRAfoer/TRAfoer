#ifndef VOXEL_LAB_SIMULATION_H
#define VOXEL_LAB_SIMULATION_H

#include "rigid.h"
#include "voxel.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VsWorld VsWorld;
enum { VS_CRATE, VS_BEAM };
typedef struct {
    RgState motion;
    /* Original voxel-space center in meters; rotate cell-reference_center,
       then add motion.position to render each cube. */
    RgVec3 reference_center;
    int voxel_count, anchored;
} VsPiece;

VsWorld *vs_create(int scene);
void vs_destroy(VsWorld *world);
const VxGraph *vs_graph(const VsWorld *world);
float vs_cell_size(void);
int vs_piece_count(const VsWorld *world);
int vs_owner(const VsWorld *world, int cell);
int vs_piece(const VsWorld *world, int piece, VsPiece *out);
RgVec3 vs_cell_position(const VsWorld *world, int cell);
void vs_step(VsWorld *world, float dt);
/* Break bonds and apply a physical blast impulse; never remove voxels.
   Cell -1 selects the current crate center (or beam midpoint). */
int vs_explode(VsWorld *world, int cell);
int vs_set_load(VsWorld *world, int total_load);
int vs_load(const VsWorld *world);
/* Apply an impulse at the selected voxel's current world center. */
int vs_impulse(VsWorld *world, int cell, RgVec3 impulse);

#ifdef __cplusplus
}
#endif
#endif
