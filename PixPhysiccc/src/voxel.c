#include "voxel.h"
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

/* The largest shell which fits is side 27: 27^3 - 25^3 = 4058 cells. */
#define VX_MAX_CRATE_SIDE 27
#define VX_MAX_GRID (VX_MAX_CRATE_SIDE * VX_MAX_CRATE_SIDE * VX_MAX_CRATE_SIDE)

static uint32_t cell_hash(int x, int y, int z) {
    uint32_t h = (uint32_t)x * UINT32_C(73856093);
    h ^= (uint32_t)y * UINT32_C(19349663);
    h ^= (uint32_t)z * UINT32_C(83492791);
    h ^= h >> 13;
    h *= UINT32_C(1274126177);
    return h ^ (h >> 16);
}

void vx_clear(VxGraph *g) {
    if (g) memset(g, 0, sizeof(*g));
}

static int panel_id(int x, int y, int z, int side) {
    if (x == 0) return 0;
    if (x == side - 1) return 1;
    if (y == 0) return 2;
    if (y == side - 1) return 3;
    if (z == 0) return 4;
    return 5;
}

static void add_bond(VxGraph *g, int a, int b) {
    VxBond *bond;
    const VxCell *cell = &g->cells[a];
    uint32_t h = cell_hash(cell->x, cell->y, cell->z) ^ (uint32_t)b * UINT32_C(2654435761);
    if (g->bond_count >= VX_MAX_BONDS) return;
    bond = &g->bonds[g->bond_count++];
    bond->a = a;
    bond->b = b;
    if (g->kind == VX_KIND_BEAM) bond->strength = 220 + (int)(h % 21);
    else bond->strength = 130;
}

static int grid_index(const VxGraph *g, int x, int y, int z) {
    return x + g->nx * (y + g->ny * z);
}

static void make_bonds(VxGraph *g) {
    int map[VX_MAX_GRID];
    int i, volume = g->nx * g->ny * g->nz;
    for (i = 0; i < volume; ++i) map[i] = -1;
    for (i = 0; i < g->cell_count; ++i) {
        const VxCell *c = &g->cells[i];
        map[grid_index(g, c->x, c->y, c->z)] = i;
    }
    for (i = 0; i < g->cell_count; ++i) {
        const VxCell *c = &g->cells[i];
        int other;
        if (c->x + 1 < g->nx && (other = map[grid_index(g, c->x + 1, c->y, c->z)]) >= 0)
            add_bond(g, i, other);
        if (c->y + 1 < g->ny && (other = map[grid_index(g, c->x, c->y + 1, c->z)]) >= 0)
            add_bond(g, i, other);
        if (c->z + 1 < g->nz && (other = map[grid_index(g, c->x, c->y, c->z + 1)]) >= 0)
            add_bond(g, i, other);
    }
}

void vx_make_crate(VxGraph *g, int side) {
    int x, y, z;
    if (!g) return;
    vx_clear(g);
    if (side <= 0) return;
    if (side > VX_MAX_CRATE_SIDE) side = VX_MAX_CRATE_SIDE;
    g->nx = g->ny = g->nz = side;
    g->kind = VX_KIND_CRATE;
    for (z = 0; z < side; ++z) for (y = 0; y < side; ++y) for (x = 0; x < side; ++x) {
        VxCell *c;
        if (x != 0 && x != side - 1 && y != 0 && y != side - 1 && z != 0 && z != side - 1) continue;
        c = &g->cells[g->cell_count++];
        c->x = x; c->y = y; c->z = z;
        c->material = panel_id(x, y, z, side);
        c->occupied = 1;
        c->integrity = 100;
    }
    make_bonds(g);
    vx_rebuild_components(g);
}

static void make_dense(VxGraph *g, int length, int width, int height, int kind) {
    int x, y, z;
    if (!g) return;
    vx_clear(g);
    if (length <= 0 || width <= 0 || height <= 0) return;
    if (length > VX_MAX_VOXELS) length = VX_MAX_VOXELS;
    if (width > VX_MAX_VOXELS) width = VX_MAX_VOXELS;
    if (height > VX_MAX_VOXELS) height = VX_MAX_VOXELS;
    /* Shrink the largest axis; never truncate the cell list into a ragged beam. */
    while ((int64_t)length * width * height > VX_MAX_VOXELS) {
        if (length >= width && length >= height) --length;
        else if (width >= height) --width;
        else --height;
    }
    g->nx = length; g->ny = width; g->nz = height;
    g->kind = kind;
    for (z = 0; z < height; ++z) for (y = 0; y < width; ++y) for (x = 0; x < length; ++x) {
        VxCell *c = &g->cells[g->cell_count++];
        c->x = x; c->y = y; c->z = z;
        c->material = 6;
        c->anchored = kind == VX_KIND_BEAM && x == 0;
        c->occupied = 1;
        c->integrity = 100;
    }
    make_bonds(g);
    vx_rebuild_components(g);
}

void vx_make_beam(VxGraph *g, int length, int width, int height) {
    make_dense(g, length, width, height, VX_KIND_BEAM);
}

void vx_make_block(VxGraph *g, int nx, int ny, int nz) {
    make_dense(g, nx, ny, nz, VX_KIND_BLOCK);
}

void vx_make_sphere(VxGraph *g, int side) {
    int x, y, z, diameter;
    if (!g) return;
    vx_clear(g);
    if (side <= 0) return;
    if (side > 20) side = 20;
    g->nx = g->ny = g->nz = side;
    g->kind = VX_KIND_SPHERE;
    diameter = side - 1;
    for (z = 0; z < side; ++z) for (y = 0; y < side; ++y) for (x = 0; x < side; ++x) {
        int dx = 2 * x - diameter, dy = 2 * y - diameter, dz = 2 * z - diameter;
        VxCell *c;
        if (dx * dx + dy * dy + dz * dz > diameter * diameter) continue;
        c = &g->cells[g->cell_count++];
        c->x = x; c->y = y; c->z = z;
        c->material = 6;
        c->occupied = 1;
        c->integrity = 100;
    }
    make_bonds(g);
    vx_rebuild_components(g);
}

static int occupied_endpoints(const VxGraph *g, const VxBond *b) {
    return b->a >= 0 && b->a < g->cell_count && b->b >= 0 && b->b < g->cell_count &&
           g->cells[b->a].occupied && g->cells[b->b].occupied;
}

int vx_rebuild_components(VxGraph *g) {
    int head[VX_MAX_VOXELS], next[2 * VX_MAX_BONDS], adjacent[2 * VX_MAX_BONDS];
    int queue[VX_MAX_VOXELS], i, edges = 0;
    if (!g) return 0;
    g->component_count = 0;
    memset(g->component_anchored, 0, sizeof(g->component_anchored));
    for (i = 0; i < g->cell_count; ++i) {
        head[i] = -1;
        g->cells[i].component = -1;
    }
    for (i = 0; i < g->bond_count; ++i) {
        const VxBond *b = &g->bonds[i];
        if (b->damage >= b->strength || !occupied_endpoints(g, b)) continue;
        adjacent[edges] = b->b; next[edges] = head[b->a]; head[b->a] = edges++;
        adjacent[edges] = b->a; next[edges] = head[b->b]; head[b->b] = edges++;
    }
    for (i = 0; i < g->cell_count; ++i) {
        int read = 0, write = 0, component;
        if (!g->cells[i].occupied || g->cells[i].component >= 0) continue;
        component = g->component_count++;
        queue[write++] = i;
        g->cells[i].component = component;
        while (read < write) {
            int cell = queue[read++], e;
            if (g->cells[cell].anchored) g->component_anchored[component] = 1;
            for (e = head[cell]; e != -1; e = next[e]) {
                int other = adjacent[e];
                if (g->cells[other].component >= 0) continue;
                g->cells[other].component = component;
                queue[write++] = other;
            }
        }
    }
    return g->component_count;
}

int vx_blast(VxGraph *g, float cx, float cy, float cz, float radius, int power) {
    int i, broken = 0;
    if (!g || power <= 0 || radius <= 0 || !isfinite(radius) || !isfinite(cx) || !isfinite(cy) || !isfinite(cz)) return 0;
    for (i = 0; i < g->bond_count; ++i) {
        VxBond *b = &g->bonds[i];
        const VxCell *a, *c;
        double dx, dy, dz, distance, damage;
        if (b->damage >= b->strength || !occupied_endpoints(g, b)) continue;
        a = &g->cells[b->a]; c = &g->cells[b->b];
        dx = ((double)a->x + c->x) * 0.5 - cx;
        dy = ((double)a->y + c->y) * 0.5 - cy;
        dz = ((double)a->z + c->z) * 0.5 - cz;
        distance = sqrt(dx * dx + dy * dy + dz * dz);
        if (distance >= radius) continue;
        /* Strength and accumulated damage are integral; the radial falloff rounds down. */
        damage = (double)power * (1.0 - distance / radius);
        if (damage >= b->strength - b->damage) {
            b->damage = b->strength;
            ++broken;
        } else b->damage += (int)damage;
    }
    return broken;
}

int vx_occupied_cells(const VxGraph *g) {
    int i, count = 0;
    if (!g) return 0;
    for (i = 0; i < g->cell_count; ++i) if (g->cells[i].occupied) ++count;
    return count;
}

static int valid_sphere(float cx, float cy, float cz, float radius) {
    return radius > 0 && isfinite(radius) && isfinite(cx) && isfinite(cy) && isfinite(cz);
}

static double cell_distance(const VxCell *cell, float cx, float cy, float cz) {
    double dx = (double)cell->x - cx;
    double dy = (double)cell->y - cy;
    double dz = (double)cell->z - cz;
    return sqrt(dx * dx + dy * dy + dz * dz);
}

static int break_removed_bonds(VxGraph *g) {
    int i, broken = 0;
    for (i = 0; i < g->bond_count; ++i) {
        VxBond *b = &g->bonds[i];
        if (b->damage >= b->strength || b->a < 0 || b->a >= g->cell_count ||
            b->b < 0 || b->b >= g->cell_count) continue;
        if (!g->cells[b->a].occupied || !g->cells[b->b].occupied) {
            b->damage = b->strength;
            b->load = 0;
            ++broken;
        }
    }
    return broken;
}

int vx_remove_sphere(VxGraph *g, float cx, float cy, float cz, float radius) {
    int i, removed = 0;
    if (!g) return 0;
    if (valid_sphere(cx, cy, cz, radius)) {
        for (i = 0; i < g->cell_count; ++i) {
            VxCell *cell = &g->cells[i];
            if (!cell->occupied || cell_distance(cell, cx, cy, cz) > radius) continue;
            cell->occupied = 0;
            cell->integrity = 0;
            ++removed;
        }
        break_removed_bonds(g);
    }
    vx_rebuild_components(g);
    return removed;
}

VxImpactResult vx_impact(VxGraph *g, float cx, float cy, float cz, float radius, int power) {
    VxImpactResult result = {0, 0, 0};
    int i;
    if (!g) return result;
    if (power > 0 && valid_sphere(cx, cy, cz, radius)) {
        for (i = 0; i < g->cell_count; ++i) {
            VxCell *cell = &g->cells[i];
            double distance, damage;
            if (!cell->occupied) continue;
            distance = cell_distance(cell, cx, cy, cz);
            if (distance >= radius) continue;
            damage = (double)power * (1.0 - distance / radius);
            if (damage < 1) continue;
            ++result.damaged_voxels;
            if (damage >= cell->integrity) {
                cell->occupied = 0;
                cell->integrity = 0;
                ++result.removed_voxels;
            } else cell->integrity -= (int)damage;
        }
        result.broken_bonds = break_removed_bonds(g);
        result.broken_bonds += vx_blast(g, cx, cy, cz, radius, power);
    }
    vx_rebuild_components(g);
    return result;
}

static int is_longitudinal(const VxGraph *g, const VxBond *b) {
    const VxCell *a = &g->cells[b->a], *c = &g->cells[b->b];
    return a->y == c->y && a->z == c->z && c->x == a->x + 1;
}

int vx_load_beam(VxGraph *g, int end_load) {
    int cells_at_x[VX_MAX_VOXELS], bonds_at_x[VX_MAX_VOXELS];
    int64_t loads_at_x[VX_MAX_VOXELS], best_load = 0;
    int component, i, broken = 0, best_component = -1, best_x = -1;
    int best_remaining = 1, supported_tip_cells = 0, tip_scale;
    if (!g) return 0;
    for (i = 0; i < g->bond_count; ++i) g->bonds[i].load = 0;
    if (g->kind != VX_KIND_BEAM || g->nx <= 0 || g->nx > VX_MAX_VOXELS) return 0;
    if (end_load < 0) end_load = 0;
    vx_rebuild_components(g);
    for (i = 0; i < g->cell_count; ++i) {
        const VxCell *c = &g->cells[i];
        if (c->occupied && c->x == g->nx - 1 && g->component_anchored[c->component])
            ++supported_tip_cells;
    }
    tip_scale = supported_tip_cells ? supported_tip_cells : 1;
    for (component = 0; component < g->component_count; ++component) {
        int x, max_x = 0, suffix_cells = 0;
        int64_t suffix_weighted = 0;
        if (!g->component_anchored[component]) continue;
        memset(cells_at_x, 0, sizeof(cells_at_x));
        memset(bonds_at_x, 0, sizeof(bonds_at_x));
        memset(loads_at_x, 0, sizeof(loads_at_x));
        for (i = 0; i < g->cell_count; ++i) {
            const VxCell *c = &g->cells[i];
            if (!c->occupied || c->component != component) continue;
            ++cells_at_x[c->x];
            ++suffix_cells;
            suffix_weighted += (int64_t)2 * c->x;
            if (c->x > max_x) max_x = c->x;
        }
        for (i = 0; i < g->bond_count; ++i) {
            const VxBond *b = &g->bonds[i];
            if (b->damage < b->strength && occupied_endpoints(g, b) &&
                g->cells[b->a].component == component && is_longitudinal(g, b))
                ++bonds_at_x[g->cells[b->a].x];
        }
        for (x = 0; x < max_x; ++x) {
            int64_t moment, denominator, demand;
            suffix_cells -= cells_at_x[x];
            suffix_weighted -= (int64_t)2 * x * cells_at_x[x];
            if (!bonds_at_x[x]) continue;
            /* Doubled lever arms place the section at x + 1/2 without fractions. */
            moment = (suffix_weighted - (int64_t)(2 * x + 1) * suffix_cells) * tip_scale;
            /* Share one total end load across supported tip voxels. Scale the entire
               moment to retain fractional component shares until final rounding. */
            if (max_x == g->nx - 1)
                moment += (int64_t)end_load * cells_at_x[g->nx - 1] * (2 * max_x - 2 * x - 1);
            denominator = (int64_t)2 * bonds_at_x[x] * tip_scale;
            demand = (moment + denominator - 1) / denominator;
            loads_at_x[x] = demand;
        }
        for (i = 0; i < g->bond_count; ++i) {
            VxBond *b = &g->bonds[i];
            int remaining;
            if (b->damage >= b->strength || !occupied_endpoints(g, b) ||
                g->cells[b->a].component != component || !is_longitudinal(g, b)) continue;
            x = g->cells[b->a].x;
            b->load = loads_at_x[x] > INT_MAX ? INT_MAX : (int)loads_at_x[x];
            remaining = b->strength - b->damage;
            if (b->load > remaining && loads_at_x[x] * best_remaining > best_load * remaining) {
                best_load = loads_at_x[x];
                best_remaining = remaining;
                best_component = component;
                best_x = x;
            }
        }
    }
    /* Fail one section in a batch, then let the rigid-body layer rebuild fragments. */
    if (best_component >= 0) for (i = 0; i < g->bond_count; ++i) {
        VxBond *b = &g->bonds[i];
        if (b->damage >= b->strength || !occupied_endpoints(g, b) ||
            g->cells[b->a].component != best_component || !is_longitudinal(g, b)) continue;
        if (g->cells[b->a].x == best_x && b->load > b->strength - b->damage) {
            b->damage = b->strength;
            ++broken;
        }
    }
    return broken;
}

int vx_settle_beam(VxGraph *g, int end_load) {
    int broken, total = 0;
    if (!g) return 0;
    do {
        broken = vx_load_beam(g, end_load);
        total += broken;
    } while (broken > 0);
    vx_rebuild_components(g);
    return total;
}

int vx_alive_bonds(const VxGraph *g) {
    int i, count = 0;
    if (!g) return 0;
    for (i = 0; i < g->bond_count; ++i)
        if (g->bonds[i].damage < g->bonds[i].strength && occupied_endpoints(g, &g->bonds[i])) ++count;
    return count;
}
