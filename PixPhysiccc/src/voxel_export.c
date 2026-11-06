#include "voxel_export.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

/* Half-full at maximum capacity, including a 4096x1x1 beam. */
#define VX_SURFACE_MAP_SIZE (VX_MAX_VOXELS * 2)

static int valid_graph(const VxGraph *graph) {
    return graph && graph->cell_count >= 0 && graph->cell_count <= VX_MAX_VOXELS &&
        graph->bond_count >= 0 && graph->bond_count <= VX_MAX_BONDS;
}

static unsigned coordinate_hash(int x, int y, int z) {
    uint32_t hash = (uint32_t)x * UINT32_C(73856093);
    hash ^= (uint32_t)y * UINT32_C(19349663);
    hash ^= (uint32_t)z * UINT32_C(83492791);
    hash ^= hash >> 16;
    return (unsigned)(hash % VX_SURFACE_MAP_SIZE);
}

static int map_find(const VxGraph *graph, const int *map, int x, int y, int z) {
    unsigned slot = coordinate_hash(x, y, z);
    while (map[slot] >= 0) {
        const VxCell *cell = &graph->cells[map[slot]];
        if (cell->x == x && cell->y == y && cell->z == z) return 1;
        slot = (slot + 1) % VX_SURFACE_MAP_SIZE;
    }
    return 0;
}

void vx_surface_stats(const VxGraph *graph, VxSurfaceStats *stats) {
    int map[VX_SURFACE_MAP_SIZE], i;
    if (!stats) return;
    *stats = (VxSurfaceStats){0, 0, 0};
    if (!valid_graph(graph)) return;
    for (i = 0; i < VX_SURFACE_MAP_SIZE; ++i) map[i] = -1;
    for (i = 0; i < graph->cell_count; ++i) {
        const VxCell *cell = &graph->cells[i];
        unsigned slot;
        if (!cell->occupied) continue;
        ++stats->occupied_voxels;
        slot = coordinate_hash(cell->x, cell->y, cell->z);
        while (map[slot] >= 0) slot = (slot + 1) % VX_SURFACE_MAP_SIZE;
        map[slot] = i;
    }
    for (i = 0; i < graph->cell_count; ++i) {
        const VxCell *cell = &graph->cells[i];
        if (!cell->occupied) continue;
        /* Positive directions visit each unordered adjacent pair once. */
        if (cell->x < INT_MAX && map_find(graph, map, cell->x + 1, cell->y, cell->z)) ++stats->shared_faces;
        if (cell->y < INT_MAX && map_find(graph, map, cell->x, cell->y + 1, cell->z)) ++stats->shared_faces;
        if (cell->z < INT_MAX && map_find(graph, map, cell->x, cell->y, cell->z + 1)) ++stats->shared_faces;
    }
    stats->exposed_faces = 6 * stats->occupied_voxels - 2 * stats->shared_faces;
}

static void write_u32(FILE *file, uint32_t value) {
    unsigned char bytes[4] = {
        (unsigned char)value, (unsigned char)(value >> 8),
        (unsigned char)(value >> 16), (unsigned char)(value >> 24)
    };
    (void)fwrite(bytes, 1, sizeof(bytes), file);
}

static void write_chunk_header(FILE *file, const char *id, uint32_t content, uint32_t children) {
    (void)fwrite(id, 1, 4, file);
    write_u32(file, content);
    write_u32(file, children);
}

static int close_output(FILE *file) {
    int ok = !ferror(file);
    if (fclose(file) != 0) ok = 0;
    return ok;
}

int vx_write_vox(const VxGraph *graph, const char *path) {
    static const unsigned char colors[7][4] = {
        {176, 112, 64, 255}, {192, 130, 76, 255}, {163, 99, 53, 255},
        {209, 149, 89, 255}, {148, 87, 44, 255}, {186, 119, 62, 255},
        {57, 192, 220, 255}
    };
    static const unsigned char neutral[4] = {180, 180, 180, 255};
    FILE *file;
    int i, occupied = 0, nx, ny, nz;
    if (!valid_graph(graph) || !path || !path[0]) return 0;
    nx = graph->nx; ny = graph->ny; nz = graph->nz;
    if (nx < 0 || ny < 0 || nz < 0 || nx > 256 || ny > 256 || nz > 256) return 0;
    for (i = 0; i < graph->cell_count; ++i) {
        const VxCell *cell = &graph->cells[i];
        if (!cell->occupied) continue;
        if (cell->x < 0 || cell->x >= nx || cell->y < 0 || cell->y >= ny ||
            cell->z < 0 || cell->z >= nz) return 0;
        ++occupied;
    }
    if (occupied && (nx == 0 || ny == 0 || nz == 0)) return 0;
    /* A cleared graph has no dimensions; SIZE still describes a valid volume. */
    if (nx == 0) nx = 1;
    if (ny == 0) ny = 1;
    if (nz == 0) nz = 1;
    file = fopen(path, "wb");
    if (!file) return 0;
    /* Primary format: github.com/ephtracy/voxel-model,
     * MagicaVoxel-file-format-vox.txt (version 150, little-endian integers).
     */
    (void)fwrite("VOX ", 1, 4, file);
    write_u32(file, 150);
    write_chunk_header(file, "MAIN", 0, 24 + 16 + 4 * (uint32_t)occupied + 1036);
    write_chunk_header(file, "SIZE", 12, 0);
    write_u32(file, (uint32_t)nx);
    write_u32(file, (uint32_t)ny);
    write_u32(file, (uint32_t)nz);
    write_chunk_header(file, "XYZI", 4 + 4 * (uint32_t)occupied, 0);
    write_u32(file, (uint32_t)occupied);
    for (i = 0; i < graph->cell_count; ++i) {
        const VxCell *cell = &graph->cells[i];
        unsigned char voxel[4];
        if (!cell->occupied) continue;
        voxel[0] = (unsigned char)cell->x;
        voxel[1] = (unsigned char)cell->y;
        voxel[2] = (unsigned char)cell->z;
        voxel[3] = (unsigned char)((uint32_t)cell->material % 255 + 1);
        (void)fwrite(voxel, 1, sizeof(voxel), file);
    }
    write_chunk_header(file, "RGBA", 1024, 0);
    for (i = 0; i < 256; ++i) {
        /* File entry zero maps to palette index one; index zero means empty. */
        (void)fwrite(i < 7 ? colors[i] : neutral, 1, 4, file);
    }
    return close_output(file);
}

int vx_write_snapshot_json(const VxGraph *graph, const char *path) {
    FILE *file;
    VxSurfaceStats stats;
    int i;
    if (!valid_graph(graph) || !path || !path[0]) return 0;
    file = fopen(path, "wb");
    if (!file) return 0;
    vx_surface_stats(graph, &stats);
    fprintf(file, "{\"format\":\"pixphysiccc.voxels.v1\","
        "\"grid\":{\"nx\":%d,\"ny\":%d,\"nz\":%d,\"voxel_size\":1.0},"
        "\"allocated_voxels\":%d,\"occupied_voxels\":%d,\"components\":%d,"
        "\"surface\":{\"exposed_faces\":%d,\"shared_faces\":%d},\"voxels\":[",
        graph->nx, graph->ny, graph->nz, graph->cell_count, stats.occupied_voxels,
        graph->component_count, stats.exposed_faces, stats.shared_faces);
    for (i = 0; i < graph->cell_count; ++i) {
        const VxCell *cell = &graph->cells[i];
        fprintf(file, "%s{\"id\":%d,\"x\":%d,\"y\":%d,\"z\":%d,\"material\":%d,"
            "\"occupied\":%s,\"integrity\":%d,\"component\":%d,\"anchored\":%s}",
            i ? "," : "", i, cell->x, cell->y, cell->z, cell->material,
            cell->occupied ? "true" : "false", cell->integrity, cell->component,
            cell->anchored ? "true" : "false");
    }
    fputs("],\"bonds\":[", file);
    for (i = 0; i < graph->bond_count; ++i) {
        const VxBond *bond = &graph->bonds[i];
        int intact = bond->a >= 0 && bond->a < graph->cell_count &&
            bond->b >= 0 && bond->b < graph->cell_count &&
            graph->cells[bond->a].occupied && graph->cells[bond->b].occupied &&
            bond->damage < bond->strength;
        fprintf(file, "%s{\"a\":%d,\"b\":%d,\"strength\":%d,\"damage\":%d,\"load\":%d,\"intact\":%s}",
            i ? "," : "", bond->a, bond->b, bond->strength, bond->damage, bond->load,
            intact ? "true" : "false");
    }
    fputs("]}\n", file);
    return close_output(file);
}
