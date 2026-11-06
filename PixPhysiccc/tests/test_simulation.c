#include "simulation.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #c); \
    exit(1); \
} } while (0)

static float length(RgVec3 v) { return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z); }
static RgVec3 sub(RgVec3 a, RgVec3 b) { return (RgVec3){a.x-b.x,a.y-b.y,a.z-b.z}; }
static void advance(VsWorld *w, int steps) { for (int i=0;i<steps;++i) vs_step(w,1.0f/120.0f); }

/* Removing material, making one body per voxel, and rendering-only displacement
   each violate at least one of these externally visible physics assertions. */
static void test_crate_explosion_preserves_material_and_moves(void)
{
    VsWorld *w = vs_create(VS_CRATE);
    CHECK(w);
    CHECK(fabsf(vs_cell_size()-0.16f)<0.00001f);
    CHECK(vx_occupied_cells(vs_graph(w)) == 1352);
    CHECK(vs_piece_count(w) == 1);
    VsPiece initial;
    CHECK(vs_piece(w,0,&initial));
    CHECK(initial.voxel_count == 1352 && !initial.anchored);
    CHECK(vs_explode(w,-1)>0);
    CHECK(vx_occupied_cells(vs_graph(w)) == 1352);
    CHECK(vs_piece_count(w)>=8 && vs_piece_count(w)<=40);
    int count=0,multi=0,moving=0,rotating=0;
    float outward=0;
    for (int p=0;p<vs_piece_count(w);++p) {
        VsPiece piece;
        CHECK(vs_piece(w,p,&piece));
        count += piece.voxel_count;
        multi += piece.voxel_count > 1;
        moving += length(piece.motion.linear_velocity)>0.1f;
        rotating += length(piece.motion.angular_velocity)>0.01f;
        RgVec3 direction=sub(piece.motion.center_of_mass,initial.motion.center_of_mass);
        outward += direction.x*piece.motion.linear_velocity.x +
                   direction.y*piece.motion.linear_velocity.y +
                   direction.z*piece.motion.linear_velocity.z;
    }
    CHECK(count==1352 && multi>=8 && moving>=8 && rotating>0 && outward>1);
    for (int c=0;c<vs_graph(w)->cell_count;++c) CHECK(vs_owner(w,c)>=0);
    advance(w,24);
    int falling=0;
    for (int p=0;p<vs_piece_count(w);++p) {
        VsPiece piece;
        CHECK(vs_piece(w,p,&piece));
        falling += piece.motion.linear_velocity.y < -0.5f;
    }
    CHECK(falling>0);
    advance(w,480);
    for (int c=0;c<vs_graph(w)->cell_count;++c) {
        RgVec3 pos=vs_cell_position(w,c);
        CHECK(isfinite(pos.x) && isfinite(pos.y) && isfinite(pos.z));
        CHECK(pos.y > -0.08f); /* physical floor, including rotated cubes */
    }
    CHECK(vx_occupied_cells(vs_graph(w)) == 1352);
    vs_destroy(w);
}

static void test_beam_load_releases_and_falls(void)
{
    VsWorld *w=vs_create(VS_BEAM);
    CHECK(w && vs_piece_count(w)==1 && vs_load(w)==0);
    const VxGraph *g=vs_graph(w);
    CHECK(vx_occupied_cells(g)==144);
    int tip=-1,anchor=-1;
    for (int c=0;c<g->cell_count;++c) {
        if (g->cells[c].x==15) tip=c;
        if (g->cells[c].anchored) anchor=c;
    }
    CHECK(tip>=0 && anchor>=0);
    RgVec3 tip_before=vs_cell_position(w,tip),anchor_before=vs_cell_position(w,anchor);
    CHECK(vs_set_load(w,100)>0 && vs_load(w)==100 && vs_piece_count(w)>1);
    VsPiece supported,detached;
    CHECK(vs_piece(w,vs_owner(w,anchor),&supported) && supported.anchored);
    CHECK(vs_piece(w,vs_owner(w,tip),&detached) && !detached.anchored);
    CHECK(length(sub(tip_before,vs_cell_position(w,tip)))<0.0001f);
    advance(w,60);
    CHECK(vs_cell_position(w,tip).y < tip_before.y-0.5f);
    CHECK(length(sub(anchor_before,vs_cell_position(w,anchor)))<0.0001f);
    CHECK(vx_occupied_cells(g)==144);
    advance(w,360);
    CHECK(vs_cell_position(w,tip).y > -0.08f);
    vs_destroy(w);
}

static void test_moving_resplit_keeps_pose_and_momentum(void)
{
    VsWorld *w=vs_create(VS_CRATE);
    CHECK(w);
    CHECK(vs_impulse(w,0,(RgVec3){70,45,30}));
    advance(w,12);
    RgVec3 before[VX_MAX_VOXELS];
    VsPiece old;
    CHECK(vs_piece(w,0,&old));
    CHECK(length(old.motion.angular_velocity)>0.01f);
    for (int c=0;c<vs_graph(w)->cell_count;++c) before[c]=vs_cell_position(w,c);
    CHECK(vs_explode(w,-1)>0);
    for (int c=0;c<vs_graph(w)->cell_count;++c)
        CHECK(length(sub(before[c],vs_cell_position(w,c)))<0.0001f);
    int biggest=0;
    for (int p=1;p<vs_piece_count(w);++p) {
        VsPiece a,b;
        CHECK(vs_piece(w,p,&a) && vs_piece(w,biggest,&b));
        if (a.voxel_count>b.voxel_count) biggest=p;
    }
    int hit=-1;
    for (int c=0;c<vs_graph(w)->cell_count;++c) {
        before[c]=vs_cell_position(w,c);
        if (vs_owner(w,c)==biggest) hit=c;
    }
    CHECK(hit>=0);
    int old_count=vs_piece_count(w);
    CHECK(vs_explode(w,hit)>0 && vs_piece_count(w)>old_count);
    for (int c=0;c<vs_graph(w)->cell_count;++c)
        CHECK(length(sub(before[c],vs_cell_position(w,c)))<0.0001f);
    CHECK(vx_occupied_cells(vs_graph(w))==1352);
    vs_destroy(w);
}

static void test_safe_invalid_arguments_and_repeated_cleanup(void)
{
    CHECK(!vs_create(-1));
    CHECK(!vs_create(2));
    CHECK(!vs_graph(NULL) && !vs_piece_count(NULL));
    CHECK(vs_owner(NULL,0)==-1);
    CHECK(!vs_piece(NULL,0,NULL));
    CHECK(!vs_explode(NULL,0) && !vs_set_load(NULL,100));
    CHECK(!vs_impulse(NULL,0,(RgVec3){1,0,0}));
    vs_step(NULL,1);
    vs_destroy(NULL);
    for (int i=0;i<3;++i) {
        VsWorld *w=vs_create(i%2);
        CHECK(w);
        CHECK(vs_owner(w,-1)==-1 && vs_owner(w,VX_MAX_VOXELS)==-1);
        CHECK(!vs_piece(w,-1,NULL) && !vs_piece(w,VX_MAX_VOXELS,NULL));
        CHECK(!vs_explode(w,-2) && !vs_explode(w,VX_MAX_VOXELS));
        CHECK(!vs_impulse(w,-1,(RgVec3){1,0,0}));
        CHECK(!vs_impulse(w,0,(RgVec3){NAN,0,0}));
        RgVec3 before=vs_cell_position(w,0);
        vs_step(w,0); vs_step(w,-1); vs_step(w,NAN);
        CHECK(length(sub(before,vs_cell_position(w,0)))<0.00001f);
        CHECK(vs_load(w)==0);
        vs_destroy(w);
    }
}

int main(void)
{
    test_crate_explosion_preserves_material_and_moves();
    test_beam_load_releases_and_falls();
    test_moving_resplit_keeps_pose_and_momentum();
    test_safe_invalid_arguments_and_repeated_cleanup();
    puts("simulation tests passed");
    return 0;
}
