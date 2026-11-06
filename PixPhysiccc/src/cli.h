#ifndef VX_CLI_H
#define VX_CLI_H

#include <stddef.h>

#define VX_CLI_MAX_ACTIONS 64
#define VX_CLI_OUTPUT_CAPACITY 1024

enum { VX_CLI_CRATE, VX_CLI_BEAM };
enum { VX_CLI_IMPACT, VX_CLI_LOAD };

typedef struct VxCliAction {
    int type;
    float x, y, z, radius;
    int value;
} VxCliAction;

typedef struct VxCliOptions {
    int scene, size, no_default, help, action_count;
    char output[VX_CLI_OUTPUT_CAPACITY];
    VxCliAction actions[VX_CLI_MAX_ACTIONS];
} VxCliOptions;

/* Returns 1 after validating all arguments; never touches the filesystem. */
int vx_cli_parse(int argc, char **argv, VxCliOptions *options, char *error, size_t error_size);

#endif
