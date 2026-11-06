#include "rigid.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
        exit(1); \
    } \
} while (0)

static const RgVec3 zero = {0, 0, 0};
static const RgQuat identity = {0, 0, 0, 1};

static void advance(RgWorld *world, int frames)
{
    for (int i = 0; i < frames; ++i) rg_step(world, 1.0f / 120.0f);
}

/* Catches missing gravity, missing floor contact, and incorrect shape extents. */
static void test_fall_rest_impulse(void)
{
    RgWorld *world = rg_create();
    CHECK(world != NULL);
    RgBody box = rg_add_voxels(world, &zero, 1, 0.5f,
                              (RgVec3){0, 5, 0}, identity, 2.0f, 0);
    CHECK(box != RG_INVALID_BODY);
    RgState state;
    CHECK(rg_get(world, box, &state));
    CHECK(fabsf(state.position.y - 5.0f) < 0.001f);
    advance(world, 30);
    CHECK(rg_get(world, box, &state));
    CHECK(state.position.y < 4.9f);
    CHECK(state.linear_velocity.y < -1.0f);
    advance(world, 900);
    CHECK(rg_get(world, box, &state));
    CHECK(state.position.y > 0.46f && state.position.y < 0.54f);
    CHECK(fabsf(state.linear_velocity.y) < 0.1f);
    rg_impulse_at(world, box, (RgVec3){4, 0, 0}, state.center_of_mass);
    CHECK(rg_get(world, box, &state));
    CHECK(fabsf(state.linear_velocity.x - 2.0f) < 0.02f);
    float old_x = state.position.x;
    advance(world, 12);
    CHECK(rg_get(world, box, &state));
    CHECK(state.position.x > old_x + 0.05f);
    rg_destroy(world);
}

/* Catches missing child colliders and confusing shape origin with COM. */
static void test_compound_and_motion(void)
{
    const RgVec3 centers[] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    RgWorld *world = rg_create();
    CHECK(world != NULL);
    RgBody body = rg_add_voxels(world, centers, 3, 0.5f,
                               (RgVec3){0, 8, 0}, identity, 3.0f, 0);
    CHECK(body != RG_INVALID_BODY);
    RgState state;
    CHECK(rg_get(world, body, &state));
    CHECK(fabsf(state.position.x) < 0.001f);
    CHECK(fabsf(state.position.y - 8.0f) < 0.001f);
    CHECK(fabsf(state.center_of_mass.x - 1.0f / 3.0f) < 0.001f);
    CHECK(fabsf(state.center_of_mass.y - (8.0f + 1.0f / 3.0f)) < 0.001f);
    rg_set_velocity(world, body, (RgVec3){1, 2, 3}, (RgVec3){0, 0, 0.5f});
    CHECK(rg_get(world, body, &state));
    CHECK(fabsf(state.linear_velocity.z - 3.0f) < 0.001f);
    CHECK(fabsf(state.angular_velocity.z - 0.5f) < 0.001f);
    rg_set_velocity(world, body, zero, zero);
    rg_impulse_at(world, body, (RgVec3){3, 0, 0},
                  (RgVec3){state.center_of_mass.x, state.center_of_mass.y + 1, 0});
    CHECK(rg_get(world, body, &state));
    CHECK(fabsf(state.linear_velocity.x - 1.0f) < 0.02f);
    CHECK(state.angular_velocity.z < -0.2f);
    advance(world, 1200);
    CHECK(rg_get(world, body, &state));
    CHECK(isfinite(state.position.x) && isfinite(state.position.y));
    CHECK(state.center_of_mass.y > 0.1f && state.center_of_mass.y < 1.6f);
    rg_destroy(world);
}

/* Catches leaked bodies, stale handle reuse, moving anchors and lifecycle errors. */
static void test_lifecycle_and_invalid_handles(void)
{
    for (int cycle = 0; cycle < 3; ++cycle) {
        RgWorld *world = rg_create();
        CHECK(world != NULL);
        RgState state;
        CHECK(!rg_get(world, RG_INVALID_BODY, &state));
        CHECK(rg_add_voxels(world, NULL, 1, 0.5f, zero, identity, 1, 0) == RG_INVALID_BODY);
        CHECK(rg_add_voxels(world, &zero, 0, 0.5f, zero, identity, 1, 0) == RG_INVALID_BODY);
        CHECK(rg_add_voxels(world, &zero, 1, -0.5f, zero, identity, 1, 0) == RG_INVALID_BODY);
        CHECK(rg_add_voxels(world, &zero, 1, 0.5f, zero, identity, 0, 0) == RG_INVALID_BODY);
        RgBody fixed = rg_add_voxels(world, &zero, 1, 0.5f,
                                    (RgVec3){0, 4, 0}, identity, 1, 1);
        CHECK(fixed != RG_INVALID_BODY);
        rg_impulse_at(world, fixed, (RgVec3){50, 50, 0}, zero);
        rg_set_velocity(world, fixed, (RgVec3){50, 50, 0}, zero);
        advance(world, 120);
        CHECK(rg_get(world, fixed, &state));
        CHECK(fabsf(state.position.y - 4.0f) < 0.001f);
        rg_remove(world, fixed);
        CHECK(!rg_get(world, fixed, &state));
        rg_remove(world, fixed);
        rg_set_velocity(world, fixed, zero, zero);
        rg_impulse_at(world, fixed, zero, zero);
        RgBody next = rg_add_voxels(world, &zero, 1, 0.5f,
                                   (RgVec3){0, 4, 0}, identity, 1, 0);
        CHECK(next != RG_INVALID_BODY);
        CHECK(!rg_get(world, fixed, &state));
        CHECK(!rg_get(world, next, NULL));
        rg_step(world, 0);
        rg_step(world, -1);
        CHECK(rg_get(world, next, &state));
        CHECK(fabsf(state.position.y - 4.0f) < 0.001f);
        rg_destroy(world);
    }
    rg_destroy(NULL);
    rg_remove(NULL, RG_INVALID_BODY);
    CHECK(!rg_get(NULL, RG_INVALID_BODY, NULL));
}

int main(void)
{
    test_fall_rest_impulse();
    test_compound_and_motion();
    test_lifecycle_and_invalid_handles();
    puts("rigid tests passed");
    return 0;
}
