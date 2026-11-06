#ifndef VOXEL_EXPORT_H
#define VOXEL_EXPORT_H

#include "voxel.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VxSurfaceStats {
    int occupied_voxels, exposed_faces, shared_faces;
} VxSurfaceStats;

/* Geometry counts occupied cubic cells, independently of structural bonds. */
void vx_surface_stats(const VxGraph *graph, VxSurfaceStats *stats);

/* Return 1 on success, 0 on invalid input or an I/O error. No viewers are opened.
 * VOX v150 is limited to dimensions 1..256 and coordinates 0..255. An empty
 * graph with zero grid dimensions is encoded as an empty 1x1x1 model.
 */
int vx_write_vox(const VxGraph *graph, const char *path);

/* JSON preserves every allocated cell ID, including unoccupied cells. */
int vx_write_snapshot_json(const VxGraph *graph, const char *path);

#ifdef __cplusplus
}
#endif
#endif
