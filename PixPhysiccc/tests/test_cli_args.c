#include "../src/cli.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static VxCliOptions parsed;
static char error[256];

static int parse(int argc, const char **argv)
{
    return vx_cli_parse(argc, (char **)argv, &parsed, error, sizeof(error));
}

static void defaults(void)
{
    const char *argv[] = {"voxel_lab"};
    assert(parse(1, argv));
    assert(parsed.scene == VX_CLI_CRATE && parsed.size == 16);
    assert(parsed.action_count == 1 && parsed.actions[0].type == VX_CLI_IMPACT);
    assert(parsed.actions[0].x == 15 && parsed.actions[0].y == 7.5f);
    assert(parsed.actions[0].radius == 3 && parsed.actions[0].value == 180);
    assert(strcmp(parsed.output, "build/results/crate") == 0);
}

static void ordered_actions(void)
{
    const char *argv[] = {"voxel_lab", "--impact", "1,2,3,4,5", "--impact", "4,3,2,1,6", "--load", "100", "--scene", "beam", "--size", "27", "--output", "nested/out"};
    assert(parse(13, argv));
    assert(parsed.scene == VX_CLI_BEAM && parsed.size == 27);
    assert(parsed.action_count == 3);
    assert(parsed.actions[0].type == VX_CLI_IMPACT && parsed.actions[0].value == 5);
    assert(parsed.actions[1].type == VX_CLI_IMPACT && parsed.actions[1].x == 4 && parsed.actions[1].value == 6);
    assert(parsed.actions[2].type == VX_CLI_LOAD && parsed.actions[2].value == 100);
    assert(strcmp(parsed.output, "nested/out") == 0);
}

static void flags(void)
{
    const char *none[] = {"voxel_lab", "--no-default"};
    const char *beam[] = {"voxel_lab", "--scene", "beam"};
    const char *help[] = {"voxel_lab", "--help", "--no-default"};
    assert(parse(2, none) && parsed.action_count == 0);
    assert(parse(3, beam) && parsed.actions[0].type == VX_CLI_LOAD && parsed.actions[0].value == 100);
    assert(strcmp(parsed.output, "build/results/beam") == 0);
    assert(parse(3, help) && parsed.help);
}

static void invalid_arguments(void)
{
    const char *cases[][5] = {
        {"voxel_lab", "--unknown", NULL},
        {"voxel_lab", "--size", NULL},
        {"voxel_lab", "--scene", "other", NULL},
        {"voxel_lab", "--size", "1", NULL},
        {"voxel_lab", "--size", "28", NULL},
        {"voxel_lab", "--size", "16garbage", NULL},
        {"voxel_lab", "--size", " 16", NULL},
        {"voxel_lab", "--size", "9999999999999999999999", NULL},
        {"voxel_lab", "--impact", "1,2,3,0,4", NULL},
        {"voxel_lab", "--impact", "1,2,3,-1,4", NULL},
        {"voxel_lab", "--impact", "nan,2,3,1,4", NULL},
        {"voxel_lab", "--impact", "1,2,3,inf,4", NULL},
        {"voxel_lab", "--impact", "1e100,2,3,1,4", NULL},
        {"voxel_lab", "--impact", "1,2,3,1,-4", NULL},
        {"voxel_lab", "--impact", "1,2,3,1,4.5", NULL},
        {"voxel_lab", "--impact", "1,2,3,1,4,", NULL},
        {"voxel_lab", "--impact", "1,2,3,1,2147483648", NULL},
        {"voxel_lab", "--carve", "1,2,3,1", NULL},
        {"voxel_lab", "--carve", "1,2,3", NULL},
        {"voxel_lab", "--carve", "1,2,3,1,4", NULL},
        {"voxel_lab", "--carve", "1,,3,1", NULL},
        {"voxel_lab", "--load", "10", NULL},
        {"voxel_lab", "--output", "", NULL},
        {"voxel_lab", "--output", "--help", NULL},
        {"voxel_lab", "--output", "--unknown", NULL},
        {"voxel_lab", "--help", "--unknown", NULL}
    };
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); ++i) {
        int argc = 0;
        while (cases[i][argc]) ++argc;
        assert(!parse(argc, cases[i]));
        assert(error[0]);
    }
}

static void boundaries(void)
{
    const char *argv[VX_CLI_MAX_ACTIONS*2 + 3];
    argv[0] = "voxel_lab";
    for (int i = 0; i < VX_CLI_MAX_ACTIONS + 1; ++i) {
        argv[1 + i*2] = "--impact";
        argv[2 + i*2] = "-1,+2,3,0.01,0";
    }
    assert(parse(1 + VX_CLI_MAX_ACTIONS*2, argv));
    assert(parsed.action_count == VX_CLI_MAX_ACTIONS);
    assert(!parse(1 + (VX_CLI_MAX_ACTIONS + 1)*2, argv));

    char path[VX_CLI_OUTPUT_CAPACITY + 1];
    memset(path, 'a', sizeof(path));
    path[sizeof(path)-1] = '\0';
    const char *long_path[] = {"voxel_lab", "--output", path};
    assert(!parse(3, long_path));
}

int main(void)
{
    defaults();
    ordered_actions();
    flags();
    invalid_arguments();
    boundaries();
    puts("CLI argument tests passed");
    return 0;
}
