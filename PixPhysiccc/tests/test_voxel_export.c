#include "voxel_export.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static VxGraph graph;
static unsigned char bytes[65536];
static int failures;
#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __func__, __LINE__, #condition); return 0; \
} } while (0)

static size_t read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    size_t size;
    if (!file) return 0;
    size = fread(bytes, 1, sizeof(bytes) - 1, file);
    bytes[size] = 0;
    fclose(file);
    return size;
}

static uint32_t read_u32(size_t offset) {
    return (uint32_t)bytes[offset] | (uint32_t)bytes[offset + 1] << 8 |
        (uint32_t)bytes[offset + 2] << 16 | (uint32_t)bytes[offset + 3] << 24;
}

/* Catches counting interior cube faces, or treating a removed corner as solid. */
static int solid_cube_surface_and_removed_corner(void) {
    VxSurfaceStats stats;
    vx_make_beam(&graph, 2, 2, 2);
    vx_surface_stats(&graph, &stats);
    CHECK(stats.occupied_voxels == 8);
    CHECK(stats.exposed_faces == 24);
    CHECK(stats.shared_faces == 12);
    graph.cells[0].occupied = 0;
    graph.cells[0].integrity = 0;
    graph.cells[0].component = -1;
    vx_surface_stats(&graph, &stats);
    CHECK(stats.occupied_voxels == 7);
    CHECK(stats.exposed_faces == 24);
    CHECK(stats.shared_faces == 9);
    return 1;
}

/* Catches deriving visible geometry from broken structural bonds. */
static int broken_bond_does_not_expose_touching_cubes(void) {
    VxSurfaceStats stats;
    vx_make_beam(&graph, 2, 1, 1);
    CHECK(graph.bond_count == 1);
    graph.bonds[0].damage = graph.bonds[0].strength;
    vx_rebuild_components(&graph);
    vx_surface_stats(&graph, &stats);
    CHECK(graph.component_count == 2);
    CHECK(stats.occupied_voxels == 2);
    CHECK(stats.exposed_faces == 10);
    CHECK(stats.shared_faces == 1);
    return 1;
}

/* Catches fixed grid maps that overflow for a long, narrow valid beam. */
static int long_beam_surface_and_empty_graph(void) {
    VxSurfaceStats stats = {1, 2, 3};
    vx_make_beam(&graph, VX_MAX_VOXELS, 1, 1);
    vx_surface_stats(&graph, &stats);
    CHECK(stats.occupied_voxels == 4096);
    CHECK(stats.exposed_faces == 16386);
    CHECK(stats.shared_faces == 4095);
    vx_clear(&graph);
    vx_surface_stats(&graph, &stats);
    CHECK(stats.occupied_voxels == 0 && stats.exposed_faces == 0 && stats.shared_faces == 0);
    stats.occupied_voxels = 99;
    vx_surface_stats(NULL, &stats);
    CHECK(stats.occupied_voxels == 0 && stats.exposed_faces == 0 && stats.shared_faces == 0);
    vx_surface_stats(&graph, NULL);
    return 1;
}

/* Catches bad magic/version, endian errors, malformed chunk lengths, duplicate
 * coordinates, removed cells being exported, and zero palette indices.
 */
static int vox_binary_contains_only_occupied_cubes(void) {
    const char *path = "build/test_export_fixture.vox";
    size_t size, offset;
    int seen_size = 0, seen_xyzi = 0, seen_palette = 0, seen[8] = {0};
    vx_make_beam(&graph, 2, 2, 2);
    graph.cells[0].occupied = 0;
    graph.cells[0].integrity = 0;
    graph.cells[0].component = -1;
    CHECK(vx_write_vox(&graph, path) == 1);
    size = read_file(path);
    CHECK(size >= 60);
    CHECK(memcmp(bytes, "VOX ", 4) == 0 && read_u32(4) == 150);
    CHECK(memcmp(bytes + 8, "MAIN", 4) == 0);
    CHECK(read_u32(12) == 0 && read_u32(16) == size - 20);
    offset = 20;
    while (offset < size) {
        uint32_t length, children;
        CHECK(size - offset >= 12);
        length = read_u32(offset + 4);
        children = read_u32(offset + 8);
        CHECK(length <= size - offset - 12 && children == 0);
        if (memcmp(bytes + offset, "SIZE", 4) == 0) {
            CHECK(!seen_size && length == 12);
            CHECK(read_u32(offset + 12) == 2 && read_u32(offset + 16) == 2 && read_u32(offset + 20) == 2);
            seen_size = 1;
        } else if (memcmp(bytes + offset, "XYZI", 4) == 0) {
            unsigned i;
            CHECK(seen_size && !seen_xyzi && length == 32);
            CHECK(read_u32(offset + 12) == 7);
            for (i = 0; i < 7; ++i) {
                size_t cell = offset + 16 + i * 4;
                unsigned x = bytes[cell], y = bytes[cell + 1], z = bytes[cell + 2];
                unsigned id;
                CHECK(x < 2 && y < 2 && z < 2);
                id = x + 2 * y + 4 * z;
                CHECK(id > 0 && seen[id] == 0);
                CHECK(bytes[cell + 3] != 0);
                seen[id] = 1;
            }
            seen_xyzi = 1;
        } else if (memcmp(bytes + offset, "RGBA", 4) == 0) {
            CHECK(!seen_palette && length == 1024);
            /* Material 6 uses index 7; RGBA entries map to indices 1..255. */
            CHECK(bytes[offset + 12 + 6 * 4 + 3] == 255);
            seen_palette = 1;
        } else CHECK(0);
        offset += 12 + length;
    }
    CHECK(offset == size && seen_size && seen_xyzi);
    CHECK(seen[1] && seen[2] && seen[3] && seen[4] && seen[5] && seen[6] && seen[7]);
    return 1;
}

/* Catches coordinate truncation and validating after an existing file is erased. */
static int vox_rejects_out_of_range_without_overwriting(void) {
    const char *path = "build/test_export_invalid.vox";
    FILE *file;
    vx_make_beam(&graph, 256, 1, 1);
    CHECK(vx_write_vox(&graph, path) == 1);
    CHECK(read_file(path) > 1080);
    file = fopen(path, "wb");
    CHECK(file != NULL);
    CHECK(fwrite("keep", 1, 4, file) == 4);
    CHECK(fclose(file) == 0);
    graph.nx = 257;
    CHECK(vx_write_vox(&graph, path) == 0);
    CHECK(read_file(path) == 4 && memcmp(bytes, "keep", 4) == 0);
    graph.nx = 256;
    graph.cells[0].x = -1;
    CHECK(vx_write_vox(&graph, path) == 0);
    graph.cells[0].x = 256;
    CHECK(vx_write_vox(&graph, path) == 0);
    graph.cells[0].x = 0;
    graph.cells[0].y = 1;
    CHECK(vx_write_vox(&graph, path) == 0);
    CHECK(read_file(path) == 4 && memcmp(bytes, "keep", 4) == 0);
    CHECK(vx_write_vox(NULL, path) == 0);
    CHECK(vx_write_vox(&graph, NULL) == 0);
    return 1;
}

/* Catches omission/renumbering of dead slots and exporting surviving bonds whose
 * endpoints were removed. Python integration additionally parses the JSON.
 */
static int json_snapshot_preserves_ids_damage_and_geometry(void) {
    const char *path = "build/test_export_fixture.json";
    vx_make_beam(&graph, 2, 1, 1);
    graph.cells[0].occupied = 0;
    graph.cells[0].integrity = 0;
    graph.cells[0].component = -1;
    graph.cells[1].integrity = 37;
    graph.bonds[0].damage = 7;
    graph.bonds[0].load = 12;
    vx_rebuild_components(&graph);
    CHECK(vx_write_snapshot_json(&graph, path) == 1);
    CHECK(read_file(path) > 0);
    CHECK(strstr((char *)bytes, "\"format\":\"pixphysiccc.voxels.v1\"") != NULL);
    CHECK(strstr((char *)bytes, "\"grid\":{\"nx\":2,\"ny\":1,\"nz\":1,\"voxel_size\":1.0}") != NULL);
    CHECK(strstr((char *)bytes, "\"allocated_voxels\":2,\"occupied_voxels\":1,\"components\":1") != NULL);
    CHECK(strstr((char *)bytes, "\"surface\":{\"exposed_faces\":6,\"shared_faces\":0}") != NULL);
    CHECK(strstr((char *)bytes, "{\"id\":0,\"x\":0,\"y\":0,\"z\":0,\"material\":6,\"occupied\":false,\"integrity\":0,\"component\":-1,\"anchored\":true}") != NULL);
    CHECK(strstr((char *)bytes, "{\"id\":1,\"x\":1,\"y\":0,\"z\":0,\"material\":6,\"occupied\":true,\"integrity\":37,\"component\":0,\"anchored\":false}") != NULL);
    CHECK(strstr((char *)bytes, "\"damage\":7,\"load\":12,\"intact\":false}") != NULL);
    vx_make_beam(&graph, 2, 1, 1);
    CHECK(vx_write_snapshot_json(&graph, path) == 1);
    CHECK(read_file(path) > 0 && strstr((char *)bytes, "\"intact\":true}") != NULL);
    graph.bonds[0].damage = graph.bonds[0].strength;
    CHECK(vx_write_snapshot_json(&graph, path) == 1);
    CHECK(read_file(path) > 0 && strstr((char *)bytes, "\"intact\":false}") != NULL);
    return 1;
}

/* Catches rejecting empty simulation snapshots or falsely reporting I/O success. */
static int empty_exports_and_io_failures(void) {
    vx_clear(&graph);
    CHECK(vx_write_vox(&graph, "build/test_export_empty.vox") == 1);
    CHECK(read_file("build/test_export_empty.vox") >= 60);
    CHECK(read_u32(32) == 1 && read_u32(36) == 1 && read_u32(40) == 1);
    CHECK(memcmp(bytes + 44, "XYZI", 4) == 0 && read_u32(48) == 4 && read_u32(56) == 0);
    CHECK(vx_write_snapshot_json(&graph, "build/test_export_empty.json") == 1);
    CHECK(read_file("build/test_export_empty.json") > 0);
    CHECK(strstr((char *)bytes, "\"voxels\":[]") != NULL);
    CHECK(strstr((char *)bytes, "\"bonds\":[]") != NULL);
    CHECK(vx_write_vox(&graph, "build/no_such_export_directory/test.vox") == 0);
    CHECK(vx_write_snapshot_json(&graph, "build/no_such_export_directory/test.json") == 0);
    CHECK(vx_write_snapshot_json(NULL, "build/test_export_invalid.json") == 0);
    CHECK(vx_write_snapshot_json(&graph, NULL) == 0);
    return 1;
}

static void run(const char *name, int (*test)(void)) {
    if (test()) printf("PASS %s\n", name);
    else ++failures;
}

int main(void) {
    run("solid cube and removed corner surface", solid_cube_surface_and_removed_corner);
    run("broken bond preserves touching cube geometry", broken_bond_does_not_expose_touching_cubes);
    run("long beam and empty surfaces", long_beam_surface_and_empty_graph);
    run("VOX byte-level cube export", vox_binary_contains_only_occupied_cubes);
    run("VOX coordinate validation", vox_rejects_out_of_range_without_overwriting);
    run("JSON IDs, damage and geometry", json_snapshot_preserves_ids_damage_and_geometry);
    run("empty exports and I/O failures", empty_exports_and_io_failures);
    return failures ? 1 : 0;
}
