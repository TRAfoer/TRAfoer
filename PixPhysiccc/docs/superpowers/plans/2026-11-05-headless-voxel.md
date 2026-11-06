# Headless Voxel Analysis Implementation Plan

> **For agentic workers:** Use scoped subagent development and verify integrated command-line behavior without any graphical launch.

**Goal:** Make voxel removal, remaining structural strength and exposed surfaces the primary results, with a no-window default CLI.

**Architecture:** Pure C occupancy/bond graph, native cubic VOX/JSON exporter and CLI. Former raylib viewer is retained as reference source outside build targets; default Make and tests require no third-party library.

**Tech Stack:** C11, GNU Make, Python integration checks; optional existing raylib/Jolt modules.

**Spec:** ../specs/2026-11-05-headless-voxel-design.md

## Global Constraints

- Never launch a window, renderer, browser or viewer.
- Work within PixPhysiccc and preserve prior optional modules.
- Integer material integrity, damage and load; stable voxel IDs, explicit occupancy.
- Default all/run/test/smoke have no raylib/OpenGL/Jolt dependency.

## Review Focus

- Removing material must remove incident structural support and change exposed geometry.
- Empty cells never appear as components or mesh faces.
- Repeated impacts and static loads must have consistent, bounded effects.
- A notch changes the remaining section's load capacity.
- Old dependency files and reference viewer source cannot pull graphics into the default build.

## Tasks

- [x] Extend and test occupancy, per-cell integrity, local impact/carving and notch-sensitive beam analysis.
- [x] Implement and test native cubic VOX export, JSON occupancy and exposed-face statistics.
- [x] Replace default main with a validated no-window CLI that writes before/after snapshots, cubic data and a summary.
- [x] Isolate the legacy viewer, update Makefile/default checks and documentation.
- [x] Run core, export and CLI scenario checks, inspect outputs, independently review and fix findings.

## Verification record

- `mingw32-make -j4 test`: 19 core test groups, seven export groups, CLI argument tests, nine end-to-end voxel scenarios and eight invalid-command cases passed.
- `make run`: native MSYS Make executed the default pure C program and wrote a crate surface impact result, 1352 -> 1348 occupied voxels, one connected component retained.
- A one-voxel notch followed by load60 produced two structural components; the intact beam survives the same load.
- Default Make/test compiled and ran in a separate source-only workspace containing no vendor directory or third-party source.
- Strict C11 syntax checks with warnings as errors passed. The default Windows binary imports only KERNEL32.dll and msvcrt.dll; no graphics library.
- Independent review found duplicated end load after longitudinal cuts created multiple supported strips. The implementation now shares one total load by surviving supported tip area with final integer rounding. Symmetric, connected-control and asymmetric cases passed; scoped independent re-review confirmed the fix.
- No viewer, renderer, browser or new graphical window was launched during this iteration.
