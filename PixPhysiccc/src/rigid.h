#ifndef VOXEL_LAB_RIGID_H
#define VOXEL_LAB_RIGID_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RgWorld RgWorld;
typedef struct { float x, y, z; } RgVec3;
typedef struct { float x, y, z, w; } RgQuat;
typedef struct {
    RgVec3 position;
    RgQuat rotation;
    RgVec3 linear_velocity, angular_velocity, center_of_mass;
} RgState;
typedef uint32_t RgBody;
#define RG_INVALID_BODY UINT32_MAX

/* The world owns a static floor with its top at y = 0. */
RgWorld *rg_create(void);
void rg_destroy(RgWorld *world);
/* Centers are local to the caller's shape origin. Mass is total body mass.
 * State.position reports that origin; velocities describe the center of mass. */
RgBody rg_add_voxels(RgWorld *world, const RgVec3 *local_centers, int count,
                     float half_size, RgVec3 position, RgQuat rotation,
                     float mass, int fixed);
void rg_remove(RgWorld *world, RgBody body);
int rg_get(RgWorld *world, RgBody body, RgState *state);
void rg_set_velocity(RgWorld *world, RgBody body, RgVec3 linear, RgVec3 angular);
void rg_impulse_at(RgWorld *world, RgBody body, RgVec3 impulse, RgVec3 world_point);
void rg_step(RgWorld *world, float dt);

#ifdef __cplusplus
}
#endif

#endif
