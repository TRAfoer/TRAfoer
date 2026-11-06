#include "cli.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int argument_error(char *error, size_t capacity, const char *message, const char *value)
{
    if (capacity) {
        if (value) snprintf(error, capacity, "%s: %s", message, value);
        else snprintf(error, capacity, "%s", message);
    }
    return 0;
}

static int parse_integer(const char *text, int minimum, int maximum, int *value)
{
    char *end;
    long number;
    if (!text[0]) return 0;
    for (const char *p = text; *p; ++p)
        if (isspace((unsigned char)*p)) return 0;
    errno = 0;
    number = strtol(text, &end, 10);
    if (errno == ERANGE || *end || number < minimum || number > maximum) return 0;
    *value = (int)number;
    return 1;
}

static int parse_impact(const char *text, VxCliAction *action)
{
    float coordinates[4];
    const char *cursor = text;
    for (const char *p = text; *p; ++p)
        if (isspace((unsigned char)*p)) return 0;
    for (int i = 0; i < 4; ++i) {
        char *end;
        errno = 0;
        coordinates[i] = strtof(cursor, &end);
        if (end == cursor || errno == ERANGE || !isfinite(coordinates[i])) return 0;
        if (*end != ',') return 0;
        cursor = end + 1;
    }
    if (coordinates[3] <= 0) return 0;
    action->type = VX_CLI_IMPACT;
    action->x = coordinates[0];
    action->y = coordinates[1];
    action->z = coordinates[2];
    action->radius = coordinates[3];
    action->value = 0;
    return parse_integer(cursor, 0, INT_MAX, &action->value);
}

int vx_cli_parse(int argc, char **argv, VxCliOptions *options, char *error, size_t error_size)
{
    int explicit_output = 0;
    memset(options, 0, sizeof(*options));
    options->size = 16;
    if (error_size) error[0] = '\0';

    for (int i = 1; i < argc; ++i) {
        const char *option = argv[i];
        const char *value;
        VxCliAction action = {0};
        if (strcmp(option, "--help") == 0) {
            options->help = 1;
            continue;
        }
        if (strcmp(option, "--no-default") == 0) {
            options->no_default = 1;
            continue;
        }
        if (strcmp(option, "--scene") && strcmp(option, "--size") &&
            strcmp(option, "--impact") &&
            strcmp(option, "--load") && strcmp(option, "--output"))
            return argument_error(error, error_size, "unknown option", option);
        if (++i == argc)
            return argument_error(error, error_size, "missing value for", option);
        value = argv[i];
        if (strncmp(value, "--", 2) == 0)
            return argument_error(error, error_size, "missing value for", option);

        if (strcmp(option, "--scene") == 0) {
            if (strcmp(value, "crate") == 0) options->scene = VX_CLI_CRATE;
            else if (strcmp(value, "beam") == 0) options->scene = VX_CLI_BEAM;
            else return argument_error(error, error_size, "scene must be crate or beam", value);
        } else if (strcmp(option, "--size") == 0) {
            if (!parse_integer(value, 2, 27, &options->size))
                return argument_error(error, error_size, "size must be an integer from 2 to 27", value);
        } else if (strcmp(option, "--output") == 0) {
            size_t length = strlen(value);
            if (!length || length >= sizeof(options->output))
                return argument_error(error, error_size, "output directory is empty or too long", NULL);
            memcpy(options->output, value, length + 1);
            explicit_output = 1;
        } else {
            if (options->action_count == VX_CLI_MAX_ACTIONS)
                return argument_error(error, error_size, "at most 64 actions are supported", NULL);
            if (strcmp(option, "--load") == 0) {
                action.type = VX_CLI_LOAD;
                if (!parse_integer(value, 0, INT_MAX, &action.value))
                    return argument_error(error, error_size, "load must be a nonnegative integer", value);
            } else if (!parse_impact(value, &action)) {
                return argument_error(error, error_size, "invalid radial action", value);
            }
            options->actions[options->action_count++] = action;
        }
    }

    for (int i = 0; i < options->action_count; ++i)
        if (options->actions[i].type == VX_CLI_LOAD && options->scene != VX_CLI_BEAM)
            return argument_error(error, error_size, "--load requires --scene beam", NULL);
    if (!explicit_output)
        snprintf(options->output, sizeof(options->output), "build/results/%s",
                 options->scene == VX_CLI_BEAM ? "beam" : "crate");
    if (!options->action_count && !options->no_default) {
        VxCliAction *action = &options->actions[options->action_count++];
        if (options->scene == VX_CLI_BEAM) {
            action->type = VX_CLI_LOAD;
            action->value = 100;
        } else {
            action->type = VX_CLI_IMPACT;
            action->x = (float)(options->size - 1);
            action->y = action->z = (options->size - 1)*0.5f;
            action->radius = 3;
            action->value = 180;
        }
    }
    return 1;
}

#ifndef VX_CLI_ARGS_ONLY

#include "voxel.h"
#include "voxel_export.h"

#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif

#define OUTPUT_PATH_CAPACITY (VX_CLI_OUTPUT_CAPACITY + 32)

typedef struct SceneStats {
    int occupied_voxels, components, live_bonds, exposed_faces;
} SceneStats;

typedef struct ActionResult {
    int type, removed_voxels, broken_bonds, components, occupied_voxels;
} ActionResult;

static VxGraph graph;

static void print_help(void)
{
    puts("PixPhysiccc - headless voxel material analysis\n"
         "Usage: voxel_lab [options]\n"
         "  --scene crate|beam       Scene (default: crate)\n"
         "  --size N                 Crate side / beam length, 2..27 (default: 16)\n"
         "  --impact X,Y,Z,R,POWER   Damage material bonds inside a sphere\n"
         "  --load N                 Settle a nonnegative beam end load\n"
         "  --no-default             Skip the built-in action when none are supplied\n"
         "  --output DIR             Output directory (default: build/results/SCENE)\n"
         "  --help                   Print this help and exit\n"
         "Repeated actions run in command-line order (maximum: 64).\n"
         "Without explicit actions, crate receives a surface impact and beam a load.\n"
         "Writes before/after .vox cube models, JSON voxel snapshots, and report.json.\n"
         "All commands run without opening a window.");
}

static int is_separator(char character)
{
#ifdef _WIN32
    return character == '/' || character == '\\';
#else
    return character == '/';
#endif
}

static int ensure_directory(const char *path)
{
#ifdef _WIN32
    struct _stat status;
    if (_stat(path, &status) == 0) {
        if (status.st_mode & _S_IFDIR) return 1;
        errno = ENOTDIR;
        return 0;
    }
    if (_mkdir(path) == 0) return 1;
    if (errno == EEXIST && _stat(path, &status) == 0 && (status.st_mode & _S_IFDIR)) return 1;
#else
    struct stat status;
    if (stat(path, &status) == 0) {
        if (S_ISDIR(status.st_mode)) return 1;
        errno = ENOTDIR;
        return 0;
    }
    if (mkdir(path, 0777) == 0) return 1;
    if (errno == EEXIST && stat(path, &status) == 0 && S_ISDIR(status.st_mode)) return 1;
#endif
    return 0;
}

static int create_directories(const char *path)
{
    char partial[VX_CLI_OUTPUT_CAPACITY];
    size_t length = strlen(path), root_end = 0;
    memcpy(partial, path, length + 1);
#ifdef _WIN32
    if (length > 1 && path[1] == ':') root_end = 2;
    else if (length > 1 && is_separator(path[0]) && is_separator(path[1])) {
        /* UNC roots consist of a server and share; create only below the share. */
        root_end = 2;
        while (root_end < length && !is_separator(path[root_end])) ++root_end;
        if (root_end < length) ++root_end;
        while (root_end < length && !is_separator(path[root_end])) ++root_end;
    }
#endif
    for (size_t i = 0; i < length; ++i) {
        if (!is_separator(partial[i]) || i <= root_end || is_separator(partial[i-1])) continue;
        partial[i] = '\0';
        if (!ensure_directory(partial)) return 0;
        partial[i] = path[i];
    }
    return ensure_directory(partial);
}

static void output_path(char *path, size_t capacity, const char *directory, const char *filename)
{
    size_t length = strlen(directory);
    snprintf(path, capacity, "%s%s%s", directory,
             is_separator(directory[length-1]) ? "" : "/", filename);
}

static int io_error(const char *path)
{
    int code = errno ? errno : EIO;
    fprintf(stderr, "error: cannot write '%s': %s\n", path, strerror(code));
    return 1;
}

static int write_snapshot(const VxCliOptions *options, const char *stage)
{
    char path[OUTPUT_PATH_CAPACITY], filename[32];
    snprintf(filename, sizeof(filename), "%s.json", stage);
    output_path(path, sizeof(path), options->output, filename);
    errno = 0;
    if (!vx_write_snapshot_json(&graph, path)) return io_error(path);
    snprintf(filename, sizeof(filename), "%s.vox", stage);
    output_path(path, sizeof(path), options->output, filename);
    errno = 0;
    if (!vx_write_vox(&graph, path)) return io_error(path);
    return 0;
}

static SceneStats scene_stats(void)
{
    VxSurfaceStats surface;
    vx_surface_stats(&graph, &surface);
    return (SceneStats){surface.occupied_voxels, graph.component_count,
                        vx_alive_bonds(&graph), surface.exposed_faces};
}

static const char *action_name(int type)
{
    return type == VX_CLI_IMPACT ? "impact" : "load";
}

static void write_stats(FILE *file, const SceneStats *stats)
{
    fprintf(file, "{\"occupied_voxels\":%d,\"components\":%d,\"live_bonds\":%d,\"exposed_faces\":%d}",
            stats->occupied_voxels, stats->components, stats->live_bonds, stats->exposed_faces);
}

static int write_report(const VxCliOptions *options, const SceneStats *initial,
                        const SceneStats *final, const ActionResult *actions)
{
    char path[OUTPUT_PATH_CAPACITY];
    output_path(path, sizeof(path), options->output, "report.json");
    errno = 0;
    FILE *file = fopen(path, "wb");
    if (!file) return io_error(path);
    fprintf(file, "{\n  \"format\":\"pixphysiccc.report.v1\",\n  \"scene\":\"%s\",\n"
                  "  \"grid\":{\"nx\":%d,\"ny\":%d,\"nz\":%d},\n  \"initial\":",
            options->scene == VX_CLI_BEAM ? "beam" : "crate", graph.nx, graph.ny, graph.nz);
    write_stats(file, initial);
    fputs(",\n  \"final\":", file);
    write_stats(file, final);
    fputs(",\n  \"actions\":[", file);
    for (int i = 0; i < options->action_count; ++i) {
        const ActionResult *action = &actions[i];
        fprintf(file, "%s\n    {\"type\":\"%s\",\"removed_voxels\":%d,\"broken_bonds\":%d,"
                      "\"components\":%d,\"occupied_voxels\":%d}",
                i ? "," : "", action_name(action->type), action->removed_voxels,
                action->broken_bonds, action->components, action->occupied_voxels);
    }
    fprintf(file, "%s],\n  \"total_removed_voxels\":%d,\n  \"total_broken_bonds\":%d,\n"
                  "  \"headless\":true,\n  \"representation\":\"voxel_cubes\"\n}\n",
            options->action_count ? "\n  " : "", initial->occupied_voxels - final->occupied_voxels,
            initial->live_bonds - final->live_bonds);
    int failed = ferror(file);
    int saved_errno = errno;
    if (fclose(file) != 0) failed = 1;
    else if (failed) errno = saved_errno;
    return failed ? io_error(path) : 0;
}

int main(int argc, char **argv)
{
    VxCliOptions options;
    ActionResult results[VX_CLI_MAX_ACTIONS];
    char error[256];
    if (!vx_cli_parse(argc, argv, &options, error, sizeof(error))) {
        fprintf(stderr, "error: %s\nUse --help for options.\n", error);
        return 2;
    }
    if (options.help) {
        print_help();
        return 0;
    }

    if (options.scene == VX_CLI_BEAM) vx_make_beam(&graph, options.size, 3, 3);
    else vx_make_crate(&graph, options.size);
    SceneStats initial = scene_stats();
    if (!create_directories(options.output)) return io_error(options.output);
    if (write_snapshot(&options, "before")) return 1;

    for (int i = 0; i < options.action_count; ++i) {
        const VxCliAction *action = &options.actions[i];
        int occupied_before = vx_occupied_cells(&graph);
        int bonds_before = vx_alive_bonds(&graph);
        if (action->type == VX_CLI_IMPACT) {
            vx_blast(&graph, action->x, action->y, action->z, action->radius, action->value);
            vx_rebuild_components(&graph);
        } else vx_settle_beam(&graph, action->value);
        results[i] = (ActionResult){action->type, occupied_before - vx_occupied_cells(&graph),
                                    bonds_before - vx_alive_bonds(&graph),
                                    graph.component_count, vx_occupied_cells(&graph)};
    }

    SceneStats final = scene_stats();
    if (write_snapshot(&options, "after")) return 1;
    if (write_report(&options, &initial, &final, results)) return 1;
    printf("%s %dx%dx%d: %d -> %d occupied voxels, %d -> %d components\n",
           options.scene == VX_CLI_BEAM ? "beam" : "crate", graph.nx, graph.ny, graph.nz,
           initial.occupied_voxels, final.occupied_voxels, initial.components, final.components);
    printf("%d broken bonds, %d exposed cube faces; %d actions\n",
           initial.live_bonds - final.live_bonds,
           final.exposed_faces, options.action_count);
    printf("Wrote voxel cubes and snapshots to %s\n", options.output);
    return 0;
}

#endif
