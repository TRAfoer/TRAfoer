#include "voxel.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static VxGraph graph;
static int failures;
#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __func__, __LINE__, #condition); return 0; \
} } while (0)

static void line_fixture(void) {
    int i;
    vx_clear(&graph);
    graph.cell_count = 4;
    graph.bond_count = 3;
    for (i = 0; i < 4; ++i) {
        graph.cells[i].x = i;
        graph.cells[i].occupied = 1;
        graph.cells[i].integrity = 100;
    }
    for (i = 0; i < 3; ++i) {
        graph.bonds[i].a = i;
        graph.bonds[i].b = i + 1;
        graph.bonds[i].strength = 100;
    }
}

/* Catches treating a damaged-but-intact edge as broken, or ignoring a cut bridge. */
static int bridge_cut_splits_and_preserves_anchor(void) {
    line_fixture();
    graph.cells[0].anchored = 1;
    graph.bonds[1].damage = 99;
    CHECK(vx_rebuild_components(&graph) == 1);
    CHECK(vx_alive_bonds(&graph) == 3);
    graph.bonds[1].damage = 100;
    CHECK(vx_rebuild_components(&graph) == 2);
    CHECK(vx_alive_bonds(&graph) == 2);
    CHECK(graph.cells[0].component == graph.cells[1].component);
    CHECK(graph.cells[2].component == graph.cells[3].component);
    CHECK(graph.cells[0].component != graph.cells[3].component);
    CHECK(graph.component_anchored[graph.cells[0].component] == 1);
    CHECK(graph.component_anchored[graph.cells[3].component] == 0);
    return 1;
}

/* Catches falloff/sign errors, cumulative blast omission, and counting old failures. */
static int blast_falloff_partial_damage_and_new_break_count(void) {
    line_fixture();
    CHECK(vx_blast(&graph, 0.5f, 0, 0, 2, 60) == 0);
    CHECK(graph.bonds[0].damage == 60);
    CHECK(graph.bonds[1].damage == 30);
    CHECK(graph.bonds[2].damage == 0);
    CHECK(vx_blast(&graph, 0.5f, 0, 0, 2, 60) == 1);
    CHECK(vx_alive_bonds(&graph) == 2);
    CHECK(vx_rebuild_components(&graph) == 2);
    CHECK(vx_blast(&graph, 0.5f, 0, 0, 0, 60) == 0);
    CHECK(vx_blast(&graph, 0.5f, 0, 0, 2, -1) == 0);
    CHECK(vx_blast(&graph, 0.5f, 0, 0, 0.1f, INT_MAX) == 0);
    CHECK(graph.bonds[0].damage == graph.bonds[0].strength);
    return 1;
}

/* Catches artificial weak tile seams opening under subthreshold material damage. */
static int crate_is_continuous_material_without_prefracture_seams(void) {
    int i;
    vx_make_crate(&graph, 8);
    CHECK(graph.cell_count == 296);
    CHECK(graph.kind == VX_KIND_CRATE);
    CHECK(vx_rebuild_components(&graph) == 1);
    for (i = 0; i < graph.cell_count; ++i) {
        VxCell *c = &graph.cells[i];
        CHECK(c->x == 0 || c->x == 7 || c->y == 0 || c->y == 7 || c->z == 0 || c->z == 7);
        CHECK(c->anchored == 0);
        CHECK(c->material >= 0 && c->material < 6);
    }
    CHECK(vx_blast(&graph, 3.5f, 3.5f, 3.5f, 1000, 50) == 0);
    CHECK(vx_rebuild_components(&graph) == 1);
    return 1;
}

/* Catches fatigue, load accumulation, and failure to split overloaded root bonds. */
static int beam_static_load_has_no_fatigue_then_root_fails(void) {
    int i, j, count, root_broken = 0;
    vx_make_beam(&graph, 16, 3, 3);
    CHECK(graph.cell_count == 144);
    CHECK(graph.kind == VX_KIND_BEAM);
    CHECK(vx_rebuild_components(&graph) == 1);
    CHECK(graph.component_anchored[0] == 1);
    count = vx_alive_bonds(&graph);
    CHECK(vx_load_beam(&graph, 0) == 0);
    for (j = 0; j < 300; ++j) CHECK(vx_load_beam(&graph, 10) == 0);
    CHECK(vx_alive_bonds(&graph) == count);
    for (i = 0; i < graph.bond_count; ++i) CHECK(graph.bonds[i].damage == 0);
    CHECK(vx_load_beam(&graph, 200) > 0);
    CHECK(vx_rebuild_components(&graph) == 2);
    for (i = 0; i < graph.bond_count; ++i) {
        VxBond *b = &graph.bonds[i];
        if (graph.cells[b->a].x == 0 && graph.cells[b->b].x == 1 && b->damage >= b->strength)
            ++root_broken;
    }
    CHECK(root_broken == 9);
    CHECK(graph.component_anchored[graph.cells[0].component] == 1);
    CHECK(graph.component_anchored[graph.cells[143].component] == 0);
    CHECK(vx_load_beam(&graph, 200) == 0);
    for (i = 0; i < graph.bond_count; ++i) CHECK(graph.bonds[i].load == 0);
    return 1;
}

/* Catches using original strength rather than remaining strength in static failure. */
static int beam_damage_reduces_capacity_without_load_fatigue(void) {
    int i, broken;
    vx_make_beam(&graph, 16, 3, 3);
    for (i = 0; i < graph.bond_count; ++i) {
        VxBond *b = &graph.bonds[i];
        if (graph.cells[b->a].x == 0 && graph.cells[b->b].x == 1)
            b->damage = b->strength - 1;
    }
    broken = vx_load_beam(&graph, 0);
    CHECK(broken == 9);
    CHECK(vx_rebuild_components(&graph) == 2);
    return 1;
}

/* Catches applying the detached tip's load to the remaining anchored stub. */
static int detached_tip_stops_transmitting_end_load(void) {
    int i;
    vx_make_beam(&graph, 16, 3, 3);
    for (i = 0; i < graph.bond_count; ++i) {
        VxBond *b = &graph.bonds[i];
        if (graph.cells[b->a].x == 7 && graph.cells[b->b].x == 8)
            b->damage = b->strength;
    }
    CHECK(vx_rebuild_components(&graph) == 2);
    CHECK(vx_load_beam(&graph, INT_MAX) == 0);
    CHECK(vx_rebuild_components(&graph) == 2);
    for (i = 0; i < graph.bond_count; ++i) {
        VxBond *b = &graph.bonds[i];
        if (graph.cells[b->a].x >= 8) CHECK(b->load == 0);
    }
    return 1;
}

/* Catches truncating 64-bit load estimates before choosing the failing section. */
static int extreme_load_still_selects_overloaded_root(void) {
    int i, root_broken = 0;
    vx_make_beam(&graph, 16, 3, 3);
    CHECK(vx_load_beam(&graph, INT_MAX) == 9);
    for (i = 0; i < graph.bond_count; ++i) {
        VxBond *b = &graph.bonds[i];
        if (graph.cells[b->a].x == 0 && graph.cells[b->b].x == 1 && b->damage >= b->strength)
            ++root_broken;
    }
    CHECK(root_broken == 9);
    CHECK(vx_rebuild_components(&graph) == 2);
    return 1;
}

/* Catches out-of-bounds generators, stale state after reset, invalid endpoint edges. */
static int generators_are_bounded_and_reset_cleanly(void) {
    int i;
    vx_make_beam(&graph, INT_MAX, INT_MAX, INT_MAX);
    CHECK(graph.cell_count > 0 && graph.cell_count <= VX_MAX_VOXELS);
    CHECK(graph.bond_count > 0 && graph.bond_count <= VX_MAX_BONDS);
    CHECK(vx_rebuild_components(&graph) == 1);
    for (i = 0; i < graph.bond_count; ++i) {
        VxBond *b = &graph.bonds[i];
        VxCell *a, *c;
        int dx, dy, dz;
        CHECK(b->a >= 0 && b->a < graph.cell_count);
        CHECK(b->b >= 0 && b->b < graph.cell_count);
        a = &graph.cells[b->a]; c = &graph.cells[b->b];
        dx = a->x - c->x; dy = a->y - c->y; dz = a->z - c->z;
        CHECK(dx * dx + dy * dy + dz * dz == 1);
    }
    vx_make_crate(&graph, INT_MAX);
    CHECK(graph.cell_count > 0 && graph.cell_count <= VX_MAX_VOXELS);
    CHECK(graph.bond_count <= VX_MAX_BONDS);
    CHECK(vx_rebuild_components(&graph) == 1);
    vx_make_crate(&graph, 1);
    CHECK(graph.cell_count == 1);
    CHECK(graph.bond_count == 0);
    CHECK(vx_rebuild_components(&graph) == 1);
    vx_make_beam(&graph, 0, 3, 3);
    CHECK(graph.cell_count == 0 && graph.bond_count == 0);
    CHECK(vx_rebuild_components(&graph) == 0);
    vx_make_crate(&graph, -1);
    CHECK(graph.cell_count == 0);
    vx_clear(&graph);
    CHECK(graph.kind == VX_KIND_EMPTY);
    CHECK(graph.component_count == 0);
    return 1;
}

/* Catches uninitialized material slots, ragged bounds and missing dense adjacency. */
static int block_is_dense_bounded_material(void) {
    int i;
    vx_make_block(&graph, 4, 3, 2);
    CHECK(graph.cell_count == 24);
    CHECK(vx_occupied_cells(&graph) == 24);
    CHECK(graph.nx == 4 && graph.ny == 3 && graph.nz == 2);
    CHECK(graph.bond_count == 46);
    CHECK(graph.component_count == 1);
    for (i = 0; i < graph.cell_count; ++i) {
        CHECK(graph.cells[i].occupied == 1);
        CHECK(graph.cells[i].integrity == 100);
    }
    vx_make_block(&graph, INT_MAX, INT_MAX, INT_MAX);
    CHECK(graph.cell_count > 0 && graph.cell_count <= VX_MAX_VOXELS);
    CHECK(graph.cell_count == graph.nx * graph.ny * graph.nz);
    CHECK(vx_occupied_cells(&graph) == graph.cell_count);
    CHECK(graph.component_count == 1);
    vx_make_block(&graph, 2, 0, 2);
    CHECK(graph.cell_count == 0 && vx_occupied_cells(&graph) == 0);
    CHECK(graph.component_count == 0);
    return 1;
}

/* Catches compacting slot IDs, leaving ghost bonds and counting empty components. */
static int carving_removes_material_and_support_with_stable_slots(void) {
    int i;
    vx_make_block(&graph, 5, 1, 1);
    graph.cells[0].anchored = 1;
    CHECK(vx_remove_sphere(&graph, 2, 0, 0, 0.1f) == 1);
    CHECK(graph.cell_count == 5 && vx_occupied_cells(&graph) == 4);
    CHECK(graph.cells[2].occupied == 0 && graph.cells[2].integrity == 0);
    CHECK(graph.cells[2].component == -1);
    CHECK(graph.cells[3].x == 3 && graph.cells[3].occupied == 1);
    CHECK(graph.component_count == 2 && vx_alive_bonds(&graph) == 2);
    CHECK(graph.component_anchored[graph.cells[1].component] == 1);
    CHECK(graph.component_anchored[graph.cells[3].component] == 0);
    CHECK(vx_remove_sphere(&graph, 2, 0, 0, 0.1f) == 0);
    CHECK(vx_remove_sphere(&graph, 2, 0, 0, 1) == 2);
    CHECK(vx_occupied_cells(&graph) == 2 && vx_alive_bonds(&graph) == 0);
    CHECK(vx_remove_sphere(&graph, 2, 0, 0, 100) == 2);
    CHECK(graph.cell_count == 5 && vx_occupied_cells(&graph) == 0);
    CHECK(graph.component_count == 0 && vx_alive_bonds(&graph) == 0);
    for (i = 0; i < graph.cell_count; ++i) CHECK(graph.cells[i].component == -1);
    CHECK(vx_remove_sphere(&graph, 2, 0, 0, 100) == 0);
    return 1;
}

/* Catches invalid sphere parameters accidentally erasing material or damaging bonds. */
static int invalid_material_actions_are_noops(void) {
    int i;
    VxImpactResult result;
    vx_make_block(&graph, 3, 1, 1);
    CHECK(vx_remove_sphere(&graph, 1, 0, 0, 0) == 0);
    CHECK(vx_remove_sphere(&graph, 1, 0, 0, -1) == 0);
    CHECK(vx_remove_sphere(&graph, 1, 0, 0, NAN) == 0);
    CHECK(vx_remove_sphere(&graph, 1, 0, 0, INFINITY) == 0);
    CHECK(vx_remove_sphere(&graph, NAN, 0, 0, 1) == 0);
    result = vx_impact(&graph, 1, 0, 0, INFINITY, INT_MAX);
    CHECK(result.damaged_voxels == 0 && result.removed_voxels == 0 && result.broken_bonds == 0);
    result = vx_impact(&graph, 1, 0, 0, 1, -1);
    CHECK(result.damaged_voxels == 0 && result.removed_voxels == 0 && result.broken_bonds == 0);
    CHECK(vx_occupied_cells(&graph) == 3 && graph.component_count == 1);
    for (i = 0; i < graph.cell_count; ++i) CHECK(graph.cells[i].integrity == 100);
    for (i = 0; i < graph.bond_count; ++i) CHECK(graph.bonds[i].damage == 0);
    return 1;
}

/* Catches missing integral falloff, resurrection, repeated removal and edge double counts. */
static int impact_accumulates_local_integrity_and_counts_only_new_damage(void) {
    VxImpactResult result;
    vx_make_block(&graph, 5, 1, 1);
    result = vx_impact(&graph, 2, 0, 0, 2, 60);
    CHECK(result.damaged_voxels == 3 && result.removed_voxels == 0);
    CHECK(result.broken_bonds == 0);
    CHECK(graph.cells[2].integrity == 40);
    CHECK(graph.cells[1].integrity == 70 && graph.cells[3].integrity == 70);
    CHECK(graph.cells[0].integrity == 100 && graph.cells[4].integrity == 100);
    result = vx_impact(&graph, 2, 0, 0, 2, 60);
    CHECK(result.damaged_voxels == 3 && result.removed_voxels == 1);
    CHECK(result.broken_bonds == 2);
    CHECK(graph.component_count == 2 && graph.cells[2].component == -1);
    result = vx_impact(&graph, 2, 0, 0, 2, 60);
    CHECK(result.damaged_voxels == 2 && result.removed_voxels == 0);
    CHECK(result.broken_bonds == 0);
    result = vx_impact(&graph, 2, 0, 0, 2, 60);
    CHECK(result.damaged_voxels == 2 && result.removed_voxels == 2);
    CHECK(result.broken_bonds == 2);
    CHECK(vx_occupied_cells(&graph) == 2 && graph.cell_count == 5);
    result = vx_impact(&graph, 2, 0, 0, 2, 60);
    CHECK(result.damaged_voxels == 0 && result.removed_voxels == 0 && result.broken_bonds == 0);
    CHECK(graph.cells[0].integrity == 100 && graph.cells[4].integrity == 100);
    return 1;
}

/* Catches omitting surviving bond damage or treating broken bonds as material holes. */
static int impact_can_break_bond_while_both_cells_survive(void) {
    VxImpactResult result;
    vx_make_block(&graph, 2, 1, 1);
    result = vx_impact(&graph, 0.5f, 0, 0, 1, 130);
    CHECK(result.damaged_voxels == 2 && result.removed_voxels == 0);
    CHECK(result.broken_bonds == 1);
    CHECK(vx_occupied_cells(&graph) == 2 && graph.component_count == 2);
    CHECK(graph.cells[0].integrity == 35 && graph.cells[1].integrity == 35);
    result = vx_impact(&graph, 0.5f, 0, 0, 1, 1);
    CHECK(result.damaged_voxels == 0 && result.removed_voxels == 0 && result.broken_bonds == 0);
    return 1;
}

/* Catches damage overflow, stale topology after full destruction and counting old breaks. */
static int extreme_impact_removes_all_material_once(void) {
    VxImpactResult result;
    int i;
    vx_make_block(&graph, 3, 2, 2);
    result = vx_impact(&graph, 1, 0, 0, 100, INT_MAX);
    CHECK(result.damaged_voxels == 12 && result.removed_voxels == 12);
    CHECK(result.broken_bonds == 20);
    CHECK(graph.cell_count == 12 && vx_occupied_cells(&graph) == 0);
    CHECK(graph.component_count == 0 && vx_alive_bonds(&graph) == 0);
    for (i = 0; i < graph.cell_count; ++i)
        CHECK(graph.cells[i].integrity == 0 && graph.cells[i].component == -1);
    result = vx_impact(&graph, 1, 0, 0, 100, INT_MAX);
    CHECK(result.damaged_voxels == 0 && result.removed_voxels == 0 && result.broken_bonds == 0);
    return 1;
}

/* Catches replacing a local hole with preset fragments or damaging distant material. */
static int crate_surface_impact_punches_local_hole(void) {
    int i, removed = 0;
    VxImpactResult result;
    vx_make_crate(&graph, 16);
    CHECK(graph.cell_count == 1352 && vx_occupied_cells(&graph) == 1352);
    result = vx_impact(&graph, 15, 7.5f, 7.5f, 3, 180);
    CHECK(result.removed_voxels > 0 && result.removed_voxels < 40);
    CHECK(result.damaged_voxels > result.removed_voxels);
    CHECK(result.broken_bonds > 0);
    CHECK(vx_occupied_cells(&graph) > 1300 && graph.cell_count == 1352);
    for (i = 0; i < graph.cell_count; ++i) {
        const VxCell *c = &graph.cells[i];
        if (!c->occupied) { CHECK(c->component == -1); ++removed; }
        if (c->x == 0) CHECK(c->occupied && c->integrity == 100);
    }
    CHECK(removed == result.removed_voxels);
    return 1;
}

/* Catches a complete carved cross-section still transmitting load through ghost cells. */
static int radial_beam_cut_disconnects_support(void) {
    vx_make_beam(&graph, 16, 3, 3);
    CHECK(vx_remove_sphere(&graph, 8, 1, 1, 1.6f) == 19);
    CHECK(graph.component_count == 2);
    CHECK(graph.component_anchored[graph.cells[0].component] == 1);
    CHECK(graph.component_anchored[graph.cells[143].component] == 0);
    CHECK(vx_settle_beam(&graph, INT_MAX) == 0);
    return 1;
}

/* Catches using original cross-section capacity or accumulating fatigue during settling. */
static int notch_reduces_static_beam_capacity_without_fatigue(void) {
    int i, intact_bonds;
    vx_make_beam(&graph, 16, 3, 3);
    intact_bonds = vx_alive_bonds(&graph);
    for (i = 0; i < 300; ++i) CHECK(vx_settle_beam(&graph, 60) == 0);
    CHECK(vx_alive_bonds(&graph) == intact_bonds);
    CHECK(vx_remove_sphere(&graph, 1, 0, 1, 0.1f) == 1);
    CHECK(vx_settle_beam(&graph, 60) > 0);
    CHECK(graph.component_anchored[graph.cells[143].component] == 0);
    CHECK(vx_settle_beam(&graph, 60) == 0);
    return 1;
}

/* Catches duplicating one total end load after a longitudinal carve splits supports. */
static int separated_supported_strips_share_one_total_end_load(void) {
    int connected, x, z, i;
    for (connected = 0; connected < 2; ++connected) {
        int loaded_bonds = 0;
        vx_make_beam(&graph, 2, 3, 3);
        for (x = 0; x < 2; ++x) for (z = 0; z < 3; ++z) {
            if (connected && x == 0 && z == 1) continue;
            CHECK(vx_remove_sphere(&graph, (float)x, 1, (float)z, 0.1f) == 1);
        }
        CHECK(graph.component_count == (connected ? 1 : 2));
        CHECK(vx_settle_beam(&graph, 2000) == 0);
        CHECK(graph.component_count == (connected ? 1 : 2));
        for (i = 0; i < graph.bond_count; ++i) {
            const VxBond *b = &graph.bonds[i];
            const VxCell *a = &graph.cells[b->a], *c = &graph.cells[b->b];
            if (!a->occupied || !c->occupied || a->x == c->x) continue;
            CHECK(b->load == 168);
            ++loaded_bonds;
        }
        CHECK(loaded_bonds == 6);
    }
    return 1;
}

/* Catches equal shares for unequal tip areas and early truncation of rational shares. */
static int unequal_supported_strips_share_load_by_surviving_tip_area(void) {
    int x, z, i, loaded_bonds = 0;
    vx_make_beam(&graph, 2, 4, 3);
    for (x = 0; x < 2; ++x) for (z = 0; z < 3; ++z)
        CHECK(vx_remove_sphere(&graph, (float)x, 1, (float)z, 0.1f) == 1);
    CHECK(graph.component_count == 2);
    CHECK(vx_settle_beam(&graph, 1000) == 0);
    for (i = 0; i < graph.bond_count; ++i) {
        const VxBond *b = &graph.bonds[i];
        const VxCell *a = &graph.cells[b->a], *c = &graph.cells[b->b];
        if (!a->occupied || !c->occupied || a->x == c->x) continue;
        CHECK(b->load == 57);
        ++loaded_bonds;
    }
    CHECK(loaded_bonds == 9);
    return 1;
}

/* Catches shell-only/smooth stand-ins, invalid sphere samples and disconnected bonds. */
static int sphere_samples_solid_voxel_material_with_bounded_connectivity(void) {
    int sides[] = {19, 20, INT_MAX};
    int s, i;
    vx_make_sphere(&graph, 3);
    CHECK(graph.kind == VX_KIND_SPHERE);
    CHECK(graph.cell_count == 7 && vx_occupied_cells(&graph) == 7);
    CHECK(graph.bond_count == 6 && graph.component_count == 1);
    vx_make_sphere(&graph, 1);
    CHECK(graph.cell_count == 1 && vx_occupied_cells(&graph) == 1);
    CHECK(graph.bond_count == 0 && graph.component_count == 1);
    CHECK(graph.cells[0].x == 0 && graph.cells[0].y == 0 && graph.cells[0].z == 0);
    for (s = 0; s < 3; ++s) {
        int side = sides[s] > 20 ? 20 : sides[s];
        vx_make_sphere(&graph, sides[s]);
        CHECK(graph.kind == VX_KIND_SPHERE);
        CHECK(graph.nx == side && graph.ny == side && graph.nz == side);
        CHECK(graph.cell_count > 2000 && graph.cell_count <= VX_MAX_VOXELS);
        CHECK(vx_occupied_cells(&graph) == graph.cell_count && graph.component_count == 1);
        for (i = 0; i < graph.cell_count; ++i) {
            const VxCell *c = &graph.cells[i];
            int dx = 2 * c->x - (side - 1), dy = 2 * c->y - (side - 1);
            int dz = 2 * c->z - (side - 1);
            CHECK(dx * dx + dy * dy + dz * dz <= (side - 1) * (side - 1));
            CHECK(c->occupied == 1 && c->integrity == 100 && c->anchored == 0);
            CHECK(c->component == 0);
        }
        for (i = 0; i < graph.bond_count; ++i) {
            const VxBond *b = &graph.bonds[i];
            const VxCell *a, *c;
            int dx, dy, dz;
            CHECK(b->a >= 0 && b->a < graph.cell_count && b->b >= 0 && b->b < graph.cell_count);
            a = &graph.cells[b->a]; c = &graph.cells[b->b];
            dx = a->x - c->x; dy = a->y - c->y; dz = a->z - c->z;
            CHECK(dx * dx + dy * dy + dz * dz == 1);
        }
    }
    vx_make_sphere(&graph, 0);
    CHECK(graph.cell_count == 0 && graph.bond_count == 0 && graph.component_count == 0);
    CHECK(graph.kind == VX_KIND_EMPTY);
    return 1;
}

static void run(const char *name, int (*test)(void)) {
    if (test()) printf("PASS %s\n", name);
    else { printf("FAIL %s\n", name); ++failures; }
}

int main(void) {
    run("bridge cut splits topology and preserves anchor", bridge_cut_splits_and_preserves_anchor);
    run("blast falloff and newly broken count", blast_falloff_partial_damage_and_new_break_count);
    run("crate material has no prefracture seams", crate_is_continuous_material_without_prefracture_seams);
    run("beam static load has no fatigue then root fails", beam_static_load_has_no_fatigue_then_root_fails);
    run("beam damage reduces remaining capacity", beam_damage_reduces_capacity_without_load_fatigue);
    run("detached tip stops transmitting load", detached_tip_stops_transmitting_end_load);
    run("extreme load selects overloaded root", extreme_load_still_selects_overloaded_root);
    run("bounded generators and clean reset", generators_are_bounded_and_reset_cleanly);
    run("dense block has bounded occupied material", block_is_dense_bounded_material);
    run("carving removes support with stable slots", carving_removes_material_and_support_with_stable_slots);
    run("invalid material actions preserve state", invalid_material_actions_are_noops);
    run("impact damage is local and cumulative", impact_accumulates_local_integrity_and_counts_only_new_damage);
    run("impact breaks bonds independently of occupancy", impact_can_break_bond_while_both_cells_survive);
    run("extreme impact removes all material once", extreme_impact_removes_all_material_once);
    run("crate surface impact punches a local hole", crate_surface_impact_punches_local_hole);
    run("radial beam cut disconnects support", radial_beam_cut_disconnects_support);
    run("notch lowers static beam capacity without fatigue", notch_reduces_static_beam_capacity_without_fatigue);
    run("separate supports share one total end load", separated_supported_strips_share_one_total_end_load);
    run("unequal supports share load by tip area", unequal_supported_strips_share_load_by_surviving_tip_area);
    run("sphere samples solid bounded voxel material", sphere_samples_solid_voxel_material_with_bounded_connectivity);
    printf("%d test failures\n", failures);
    return failures ? 1 : 0;
}
