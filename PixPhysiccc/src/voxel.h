#ifndef VOXEL_H
#define VOXEL_H

#ifdef __cplusplus
extern "C" {
#endif

#define VX_MAX_VOXELS 4096
#define VX_MAX_BONDS 12288

enum { VX_KIND_EMPTY, VX_KIND_CRATE, VX_KIND_BEAM, VX_KIND_BLOCK, VX_KIND_SPHERE };

typedef struct VxCell {
    int x, y, z, material, anchored, component;
    int occupied, integrity;
} VxCell;

typedef struct VxBond {
    int a, b, strength, damage, load;
} VxBond;

typedef struct VxGraph {
    VxCell cells[VX_MAX_VOXELS];
    VxBond bonds[VX_MAX_BONDS];
    int cell_count, bond_count, component_count;
    int component_anchored[VX_MAX_VOXELS];
    int nx, ny, nz, kind;
} VxGraph;

typedef struct {
    int damaged_voxels, removed_voxels, broken_bonds;
} VxImpactResult;

void vx_clear(VxGraph *graph);
/* Nonpositive sizes clear the graph; oversized requests are bounded to capacity. */
void vx_make_crate(VxGraph *graph, int side);
void vx_make_beam(VxGraph *graph, int length, int width, int height);
void vx_make_block(VxGraph *graph, int nx, int ny, int nz);
/* Fits integer voxel centers inside a sphere; side is bounded to 20. */
void vx_make_sphere(VxGraph *graph, int side);
int vx_occupied_cells(const VxGraph *graph);
/* Cell slots stay stable; removal clears occupancy/integrity and incident bonds. */
int vx_remove_sphere(VxGraph *graph, float cx, float cy, float cz, float radius);
/* damaged_voxels includes newly removed cells; all counts describe this hit only. */
VxImpactResult vx_impact(VxGraph *graph, float cx, float cy, float cz, float radius, int power);
int vx_rebuild_components(VxGraph *graph);
/* Returns newly broken bonds; call rebuild before consuming component labels. */
int vx_blast(VxGraph *graph, float cx, float cy, float cz, float radius, int power);
/* Shares total end load across supported tip voxels, without fatigue;
   at most one overloaded section fails per call. */
int vx_load_beam(VxGraph *graph, int end_load);
/* Repeats static load evaluation to equilibrium and rebuilds component labels. */
int vx_settle_beam(VxGraph *graph, int end_load);
int vx_alive_bonds(const VxGraph *graph);

#ifdef __cplusplus
}
#endif
#endif
