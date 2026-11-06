#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "simulation.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Cubic faces and picking both use the current rigid-fragment pose. */
#define MAP_SIDE 32
#define HEADER_HEIGHT 88
#define FOOTER_HEIGHT 116
#define FIXED_STEP (1.0f / 120.0f)
#define CAPTURE_FRAMES 300

static VsWorld *world;
static const VxGraph *graph;
static VsPiece pieces[VX_MAX_VOXELS];
static Vector3 centers[VX_MAX_VOXELS], initial_centers[VX_MAX_VOXELS];
static Quaternion rotations[VX_MAX_VOXELS];
static int owners[VX_MAX_VOXELS], cell_map[MAP_SIDE * MAP_SIDE * MAP_SIDE];
static float cell_stress[VX_MAX_VOXELS];
static Camera3D camera;
static int scene, piece_count, selected_cell = -1, occupied;
static int show_components, show_grid, show_stress, paused;
static int capture_mode, frame, screenshots_written, capture_failed, invalid_states;
static int crate_initial_voxels, crate_final_voxels, crate_initial_components, crate_final_components;
static int beam_initial_components, beam_final_components;
static float crate_max_displacement, crate_max_rotation, beam_drop;
static float yaw = 0.63f, pitch = 0.41f, distance = 9.2f, accumulator;
static char action_status[180];

static const Color BG = {19, 26, 34, 255};
static const Color PANEL = {22, 30, 39, 255};
static const Color INK = {230, 234, 235, 255};
static const Color MUTED = {140, 158, 171, 255};
static const Color LINE = {48, 63, 75, 255};
static const Color ACCENT = {236, 179, 108, 255};
static const Color CYAN = {111, 207, 196, 255};

static Vector3 vector(RgVec3 v) { return (Vector3){v.x, v.y, v.z}; }
static Quaternion quaternion(RgQuat q) { return (Quaternion){q.x, q.y, q.z, q.w}; }

static int finite_vector(RgVec3 v)
{
    return isfinite(v.x) && isfinite(v.y) && isfinite(v.z);
}

static int valid_state(const RgState *s)
{
    return finite_vector(s->position) && finite_vector(s->center_of_mass) &&
           finite_vector(s->linear_velocity) && finite_vector(s->angular_velocity) &&
           isfinite(s->rotation.x) && isfinite(s->rotation.y) &&
           isfinite(s->rotation.z) && isfinite(s->rotation.w);
}

static int map_index(int x, int y, int z) { return x + MAP_SIDE * (y + MAP_SIDE * z); }

static int cell_at(int x, int y, int z)
{
    if (x < 0 || x >= MAP_SIDE || y < 0 || y >= MAP_SIDE || z < 0 || z >= MAP_SIDE) return -1;
    return cell_map[map_index(x, y, z)];
}

static void read_poses(void)
{
    graph = vs_graph(world);
    piece_count = vs_piece_count(world);
    for (int p = 0; p < piece_count; ++p) {
        if (!vs_piece(world, p, &pieces[p]) || !valid_state(&pieces[p].motion)) {
            ++invalid_states;
            memset(&pieces[p], 0, sizeof(pieces[p]));
            pieces[p].motion.rotation.w = 1;
        }
        rotations[p] = quaternion(pieces[p].motion.rotation);
    }
    for (int i = 0; i < graph->cell_count; ++i) {
        owners[i] = vs_owner(world, i);
        if (!graph->cells[i].occupied) continue;
        if (owners[i] < 0 || owners[i] >= piece_count) { ++invalid_states; continue; }
        const VxCell *c = &graph->cells[i];
        Vector3 raw = {c->x * vs_cell_size(), c->y * vs_cell_size(), c->z * vs_cell_size()};
        const VsPiece *piece = &pieces[owners[i]];
        centers[i] = Vector3Add(vector(piece->motion.position),
            Vector3RotateByQuaternion(Vector3Subtract(raw, vector(piece->reference_center)), rotations[owners[i]]));
    }
}

static void refresh_structure(void)
{
    graph = vs_graph(world);
    occupied = vx_occupied_cells(graph);
    memset(cell_map, -1, sizeof(cell_map));
    memset(cell_stress, 0, sizeof(cell_stress));
    for (int i = 0; i < graph->cell_count; ++i) {
        const VxCell *c = &graph->cells[i];
        if (c->occupied) cell_map[map_index(c->x, c->y, c->z)] = i;
    }
    for (int i = 0; i < graph->bond_count; ++i) {
        const VxBond *b = &graph->bonds[i];
        float damage = (float)b->damage / b->strength;
        float load = b->damage < b->strength ? (float)b->load / (b->strength - b->damage) : 1;
        float stress = fmaxf(damage, load);
        cell_stress[b->a] = fmaxf(cell_stress[b->a], stress);
        cell_stress[b->b] = fmaxf(cell_stress[b->b], stress);
    }
    read_poses();
}

static int reset_scene(int next)
{
    VsWorld *replacement = vs_create(next);
    if (!replacement) {
        snprintf(action_status, sizeof(action_status), "Could not create the physics scene");
        capture_failed = 1;
        return 0;
    }
    vs_destroy(world);
    world = replacement;
    scene = next;
    paused = 0;
    accumulator = 0;
    selected_cell = -1;
    yaw = scene == VS_BEAM ? 0.47f : 0.63f;
    pitch = scene == VS_BEAM ? 0.30f : 0.41f;
    distance = scene == VS_BEAM ? 7.8f : 9.2f;
    refresh_structure();
    memcpy(initial_centers, centers, sizeof(initial_centers));
    snprintf(action_status, sizeof(action_status), scene == VS_CRATE ?
             "Intact 16 x 16 x 16 voxel shell / SPACE to explode" :
             "Anchored at left / SPACE adds 25 end load");
    return 1;
}

static void apply_load(int delta)
{
    if (scene != VS_BEAM) return;
    int load = vs_load(world) + delta;
    if (load < 0) load = 0;
    int broken = vs_set_load(world, load);
    refresh_structure();
    snprintf(action_status, sizeof(action_status), "End load %d / %d broken bonds / %d moving pieces",
             vs_load(world), broken, piece_count);
}

static void impact(int cell)
{
    int broken = vs_explode(world, cell);
    refresh_structure();
    if (cell >= 0) {
        Vector3 direction = Vector3Normalize(Vector3Subtract(centers[cell], camera.position));
        direction = Vector3Scale(direction, 1.4f);
        vs_impulse(world, cell, (RgVec3){direction.x, direction.y + 0.4f, direction.z});
        snprintf(action_status, sizeof(action_status), "Local impact / %d broken bonds / %d rigid fragments", broken, piece_count);
    } else snprintf(action_status, sizeof(action_status), "Explosion / %d broken bonds / %d rigid fragments", broken, piece_count);
}

static void scene_action(void)
{
    if (scene == VS_BEAM) apply_load(25);
    else impact(-1);
}

static void update_camera(void)
{
    if (!capture_mode && IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        Vector2 delta = GetMouseDelta();
        yaw -= delta.x * 0.006f;
        pitch = Clamp(pitch + delta.y * 0.006f, 0.08f, 1.40f);
    }
    if (!capture_mode) distance = Clamp(distance - GetMouseWheelMove() * 0.5f, 3.5f, 22.0f);
    camera.target = (Vector3){0, scene == VS_BEAM ? 1.35f : 1.05f, 0};
    camera.position = Vector3Add(camera.target,
        (Vector3){sinf(yaw) * cosf(pitch) * distance, sinf(pitch) * distance, cosf(yaw) * cosf(pitch) * distance});
    camera.up = (Vector3){0, 1, 0};
    camera.fovy = 38;
    camera.projection = CAMERA_PERSPECTIVE;
}

static int pick_cell(Ray ray)
{
    float nearest = FLT_MAX, half = vs_cell_size() * 0.5f;
    int picked = -1;
    BoundingBox box = {{-half, -half, -half}, {half, half, half}};
    for (int i = 0; i < graph->cell_count; ++i) {
        if (!graph->cells[i].occupied || owners[i] < 0) continue;
        Quaternion inverse = QuaternionInvert(rotations[owners[i]]);
        Ray local = {Vector3RotateByQuaternion(Vector3Subtract(ray.position, centers[i]), inverse),
                     Vector3RotateByQuaternion(ray.direction, inverse)};
        RayCollision hit = GetRayCollisionBox(local, box);
        if (hit.hit && hit.distance < nearest) { nearest = hit.distance; picked = i; }
    }
    return picked;
}

static Color scaled_color(Color color, float factor)
{
    color.r = (unsigned char)Clamp(color.r * factor, 0, 255);
    color.g = (unsigned char)Clamp(color.g * factor, 0, 255);
    color.b = (unsigned char)Clamp(color.b * factor, 0, 255);
    return color;
}

static Color cell_color(int i)
{
    const VxCell *c = &graph->cells[i];
    if (show_stress) {
        float t = Clamp(cell_stress[i], 0, 1);
        return (Color){(unsigned char)(98 + 147 * t), (unsigned char)(187 - 107 * t),
                       (unsigned char)(176 - 111 * t), 255};
    }
    if (show_components) {
        static const Color colors[] = {{213, 169, 108, 255}, {102, 185, 184, 255},
            {206, 119, 104, 255}, {151, 144, 197, 255}, {153, 182, 104, 255}, {215, 152, 173, 255}};
        return colors[owners[i] % 6];
    }
    if (scene == VS_BEAM) return c->anchored ? (Color){118, 145, 157, 255} : (Color){141, 172, 185, 255};
    float strip = (c->y / 4) % 2 ? 0.97f : 1.02f;
    return scaled_color((Color){199, 151, 96, 255}, strip);
}

static Vector3 transformed_offset(int cell, Vector3 offset)
{
    return Vector3Add(centers[cell], Vector3RotateByQuaternion(offset, rotations[owners[cell]]));
}

static void draw_cell_wire(int cell, float scale, Color color)
{
    Vector3 corners[8];
    float half = vs_cell_size() * 0.5f * scale;
    for (int i = 0; i < 8; ++i) corners[i] = transformed_offset(cell,
        (Vector3){i & 1 ? half : -half, i & 2 ? half : -half, i & 4 ? half : -half});
    for (int i = 0; i < 8; ++i) for (int axis = 1; axis <= 4; axis *= 2)
        if (!(i & axis)) DrawLine3D(corners[i], corners[i | axis], color);
}

static void draw_voxels(void)
{
    static const int directions[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    static const float vertices[6][4][3] = {
        {{1,-1,-1},{1,1,-1},{1,1,1},{1,-1,1}}, {{-1,-1,1},{-1,1,1},{-1,1,-1},{-1,-1,-1}},
        {{-1,1,-1},{-1,1,1},{1,1,1},{1,1,-1}}, {{-1,-1,1},{-1,-1,-1},{1,-1,-1},{1,-1,1}},
        {{1,-1,1},{1,1,1},{-1,1,1},{-1,-1,1}}, {{-1,-1,-1},{-1,1,-1},{1,1,-1},{1,-1,-1}}
    };
    float half = vs_cell_size() * 0.5f;
    Vector3 light = Vector3Normalize((Vector3){-0.25f, 0.8f, 1});
    rlDisableBackfaceCulling();
    rlBegin(RL_QUADS);
    for (int i = 0; i < graph->cell_count; ++i) {
        const VxCell *c = &graph->cells[i];
        if (!c->occupied || owners[i] < 0) continue;
        Color base = cell_color(i);
        for (int face = 0; face < 6; ++face) {
            int other = cell_at(c->x + directions[face][0], c->y + directions[face][1], c->z + directions[face][2]);
            if (other >= 0 && owners[other] == owners[i]) continue;
            Vector3 normal = Vector3RotateByQuaternion((Vector3){(float)directions[face][0],
                (float)directions[face][1], (float)directions[face][2]}, rotations[owners[i]]);
            float shade = 0.62f + 0.57f * fmaxf(0, Vector3DotProduct(normal, light));
            Color color = scaled_color(base, shade);
            rlColor4ub(color.r, color.g, color.b, 255);
            for (int v = 0; v < 4; ++v) {
                Vector3 p = transformed_offset(i, (Vector3){vertices[face][v][0] * half,
                    vertices[face][v][1] * half, vertices[face][v][2] * half});
                rlVertex3f(p.x, p.y, p.z);
            }
        }
    }
    rlEnd();
    rlEnableBackfaceCulling();
    if (show_grid) for (int i = 0; i < graph->cell_count; ++i)
        if (graph->cells[i].occupied && owners[i] >= 0) draw_cell_wire(i, 1.001f, (Color){45, 51, 52, 100});
    if (selected_cell >= 0) draw_cell_wire(selected_cell, 1.025f, (Color){255, 237, 193, 255});
}

static void draw_scene(void)
{
    BeginMode3D(camera);
    DrawPlane((Vector3){0, -0.012f, 0}, (Vector2){100, 100}, (Color){22, 31, 39, 255});
    for (int line = -16; line <= 16; ++line) {
        float p = line * 0.5f;
        Color color = line == 0 ? (Color){54, 72, 83, 255} : (Color){33, 47, 58, 255};
        DrawLine3D((Vector3){p, 0, -8}, (Vector3){p, 0, 8}, color);
        DrawLine3D((Vector3){-8, 0, p}, (Vector3){8, 0, p}, color);
    }
    if (scene == VS_BEAM) {
        float anchor_x = -(graph->nx - 1) * vs_cell_size() * 0.5f;
        DrawCube((Vector3){anchor_x - 0.12f, 0.92f, 0}, 0.42f, 1.84f, 0.90f, (Color){54, 72, 84, 255});
        DrawCube((Vector3){anchor_x - 0.12f, 1.86f, 0}, 0.55f, 0.10f, 1.02f, (Color){81, 103, 114, 255});
    }
    draw_voxels();
    if (scene == VS_BEAM && piece_count == 1) {
        Vector3 tip = {(graph->nx - 1) * vs_cell_size() * 0.5f, 2.92f, 0};
        Vector3 bottom = {tip.x, 2.59f, tip.z};
        DrawCylinderEx(tip, bottom, 0.016f, 0.016f, 8, ACCENT);
        DrawCylinderEx(bottom, (Vector3){tip.x, 2.45f, tip.z}, 0.08f, 0, 10, ACCENT);
    }
    EndMode3D();
    if (show_stress) DrawText("H  BOND STRESS", 28, HEADER_HEIGHT + 18, 14, CYAN);
    if (paused) DrawText("PAUSED", GetScreenWidth() / 2 - 35, HEADER_HEIGHT + 18, 18, ACCENT);
}

static Rectangle scene_button(int index)
{
    return (Rectangle){GetScreenWidth() - 328.0f + index * 150.0f, 23, 142, 40};
}

static void draw_stat(int x, int y, const char *label, int value, Color color)
{
    DrawText(label, x, y, 11, MUTED);
    DrawText(TextFormat("%d", value), x, y + 17, 25, color);
}

static void draw_interface(void)
{
    int width = GetScreenWidth(), height = GetScreenHeight(), top = height - FOOTER_HEIGHT;
    DrawRectangle(0, 0, width, HEADER_HEIGHT, PANEL);
    DrawLine(0, HEADER_HEIGHT - 1, width, HEADER_HEIGHT - 1, LINE);
    DrawText("PIXPHYSICCC", 28, 20, 22, INK);
    DrawText("CUBIC FRACTURE / RIGID BODY PHYSICS", 28, 51, 11, MUTED);
    const char *labels[2] = {"1  CRATE", "2  BEAM"};
    for (int i = 0; i < 2; ++i) {
        Rectangle r = scene_button(i);
        int active = scene == i;
        DrawRectangleRec(r, active ? (Color){58, 70, 77, 255} : (Color){31, 42, 53, 255});
        if (active) DrawRectangle((int)r.x, (int)(r.y + r.height - 2), (int)r.width, 2, ACCENT);
        DrawText(labels[i], (int)r.x + ((int)r.width - MeasureText(labels[i], 13)) / 2, (int)r.y + 13,
                 13, active ? INK : MUTED);
    }
    DrawRectangle(0, top, width, FOOTER_HEIGHT, PANEL);
    DrawLine(28, top, width - 28, top, LINE);
    draw_stat(28, top + 13, "VOXELS", occupied, INK);
    draw_stat(182, top + 13, "LIVE BONDS", vx_alive_bonds(graph), CYAN);
    draw_stat(320, top + 13, "FRAGMENTS", piece_count, ACCENT);
    DrawText(action_status, 476, top + 17, 13, INK);
    const char *state = scene == VS_BEAM ? TextFormat("END LOAD  %d", vs_load(world)) : "GRAVITY / COLLISION / ANGULAR MOTION";
    DrawText(state, 476, top + 39, 12, MUTED);
    DrawText(scene == VS_CRATE ? "SPACE  Explode    LMB  Local impact    R  Reset    P  Pause" :
             "SPACE / +  Add 25 load    -  Reduce load    LMB  Local impact    R  Reset    P  Pause",
             28, height - 41, 13, MUTED);
    DrawText("RMB drag  Orbit    Wheel  Zoom    C  Fragment colors    G  Voxel grid    H  Stress", 28, height - 21, 12, MUTED);
    if (selected_cell >= 0) {
        const VxCell *c = &graph->cells[selected_cell];
        const char *hover = TextFormat("VOXEL [%d, %d, %d] / fragment %d", c->x, c->y, c->z, owners[selected_cell]);
        DrawText(hover, width - MeasureText(hover, 12) - 28, HEADER_HEIGHT + 18, 12, MUTED);
    }
}

static void handle_input(void)
{
    if (IsKeyPressed(KEY_ONE)) reset_scene(VS_CRATE);
    if (IsKeyPressed(KEY_TWO)) reset_scene(VS_BEAM);
    if (IsKeyPressed(KEY_R)) reset_scene(scene);
    if (IsKeyPressed(KEY_P)) paused = !paused;
    if (IsKeyPressed(KEY_C)) show_components = !show_components;
    if (IsKeyPressed(KEY_G)) show_grid = !show_grid;
    if (IsKeyPressed(KEY_H)) show_stress = !show_stress;
    if (IsKeyPressed(KEY_SPACE)) scene_action();
    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) apply_load(25);
    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) apply_load(-25);
    Vector2 mouse = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        for (int i = 0; i < 2; ++i) if (CheckCollisionPointRec(mouse, scene_button(i))) reset_scene(i);
}

static void capture_before_frame(void)
{
    if (frame == 1) {
        crate_initial_voxels = occupied;
        crate_initial_components = piece_count;
    }
    if (frame == 35) impact(-1);
    if (frame == 100) {
        crate_final_voxels = occupied;
        crate_final_components = piece_count;
        reset_scene(VS_BEAM);
        beam_initial_components = piece_count;
    }
    if (frame == 120) { apply_load(300); beam_final_components = piece_count; }
}

static void capture_metrics(void)
{
    if (scene == VS_CRATE) {
        for (int i = 0; i < graph->cell_count; ++i) {
            float moved = Vector3Distance(initial_centers[i], centers[i]);
            crate_max_displacement = fmaxf(crate_max_displacement, moved);
        }
        for (int p = 0; p < piece_count; ++p) {
            float angle = 2 * acosf(Clamp(fabsf(rotations[p].w), 0, 1));
            crate_max_rotation = fmaxf(crate_max_rotation, angle);
        }
    } else if (piece_count > 1) {
        float total_drop = 0;
        int count = 0;
        for (int i = 0; i < graph->cell_count; ++i) if (owners[i] >= 0 && !pieces[owners[i]].anchored) {
            total_drop += initial_centers[i].y - centers[i].y;
            ++count;
        }
        if (count) beam_drop = fmaxf(beam_drop, total_drop / count);
    }
}

static void capture_after_frame(void)
{
    const char *path = NULL;
    if (frame == 20) path = "build/viewer_crate.png";
    if (frame == 75) path = "build/viewer_blast.png";
    if (frame == 150) path = "build/viewer_beam.png";
    if (frame == CAPTURE_FRAMES) path = "build/viewer_settled.png";
    if (!path) return;
    Image screenshot = LoadImageFromScreen();
    if (ExportImage(screenshot, path)) ++screenshots_written;
    else capture_failed = 1;
    UnloadImage(screenshot);
}

static int write_capture_report(void)
{
    FILE *report = fopen("build/viewer_report.json", "w");
    if (!report) return 0;
    int written = fprintf(report,
        "{\n  \"crate_initial_voxels\": %d,\n  \"crate_final_voxels\": %d,\n"
        "  \"crate_initial_components\": %d,\n  \"crate_final_components\": %d,\n"
        "  \"crate_max_displacement\": %.6f,\n  \"crate_max_rotation\": %.6f,\n"
        "  \"beam_initial_components\": %d,\n  \"beam_final_components\": %d,\n"
        "  \"beam_drop\": %.6f,\n  \"invalid_states\": %d,\n"
        "  \"screenshots_written\": %d\n}\n",
        crate_initial_voxels, crate_final_voxels, crate_initial_components, crate_final_components,
        crate_max_displacement, crate_max_rotation, beam_initial_components, beam_final_components,
        beam_drop, invalid_states, screenshots_written);
    return fclose(report) == 0 && written > 0;
}

int main(int argc, char **argv)
{
    int initial_scene = VS_CRATE;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--capture") == 0) capture_mode = 1;
        else if (strcmp(argv[i], "--scene") == 0 && i + 1 < argc) {
            const char *name = argv[++i];
            if (strcmp(name, "crate") == 0) initial_scene = VS_CRATE;
            else if (strcmp(name, "beam") == 0) initial_scene = VS_BEAM;
            else { fprintf(stderr, "Unknown scene: %s\n", name); return 2; }
        } else if (strcmp(argv[i], "--help") == 0) {
            puts("PixPhysiccc cubic fracture viewer (C + raylib + Jolt physics).");
            puts("1/2: crate/beam; RMB drag: orbit; wheel: zoom; LMB: local physical impact.");
            puts("SPACE: explosion or beam load+25; +/-: beam load; R: reset; P: pause.");
            puts("C: fragment colors; G: voxel grid; H: bond stress.");
            puts("--scene crate|beam selects the initial scene.");
            puts("--capture opens a window and records four physics screenshots plus build/viewer_report.json.");
            return 0;
        } else {
            fprintf(stderr, "Unknown viewer option: %s\nUse --help for controls.\n", argv[i]);
            return 2;
        }
    }
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 800, "PixPhysiccc | Cubic fracture physics");
    if (!IsWindowReady()) return 1;
    SetWindowMinSize(1060, 680);
    SetTargetFPS(capture_mode ? 120 : 60);
    if (!reset_scene(capture_mode ? VS_CRATE : initial_scene)) { CloseWindow(); return 1; }
    while (!WindowShouldClose() && (!capture_mode || frame < CAPTURE_FRAMES)) {
        ++frame;
        if (capture_mode) capture_before_frame();
        else handle_input();
        update_camera();
        if (!paused) {
            float dt = capture_mode ? 1.0f / 60.0f : fminf(GetFrameTime(), 0.1f);
            accumulator += dt;
            while (accumulator >= FIXED_STEP) { vs_step(world, FIXED_STEP); accumulator -= FIXED_STEP; }
        }
        read_poses();
        if (capture_mode) capture_metrics();
        selected_cell = -1;
        Vector2 mouse = GetMousePosition();
        if (!capture_mode && mouse.y > HEADER_HEIGHT && mouse.y < GetScreenHeight() - FOOTER_HEIGHT) {
            selected_cell = pick_cell(GetScreenToWorldRay(mouse, camera));
            if (selected_cell >= 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { impact(selected_cell); selected_cell = -1; }
        }
        BeginDrawing();
        ClearBackground(BG);
        draw_scene();
        draw_interface();
        EndDrawing();
        if (capture_mode) capture_after_frame();
    }
    if (capture_mode && !write_capture_report()) capture_failed = 1;
    vs_destroy(world);
    CloseWindow();
    return capture_failed || invalid_states || (capture_mode && screenshots_written != 4) ? 1 : 0;
}
