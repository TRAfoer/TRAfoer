# Voxel Scene Display Implementation Plan

**Goal:** Open an interactive scene using occupied cubes to fit the shape and its damaged boundary, as requested after the headless core iteration.

**Design:** An explicit C/raylib viewer consumes the same occupancy, damage and component graph as the CLI. Draw touching full-size cubes without default per-cell outlines. A cut crate, a loaded beam and a voxel-sampled sphere show holes, cracks and staircase contours. There is no external mesh loading or generic falling-fragment demonstration. Keep all/run/test/smoke headless; only explicit view/test-viewer targets open a window.

- [x] Add and test a bounded sphere occupancy generator for the curved shape sample.
- [x] Replace the reference viewer with the current voxel-data display, picking, carve/impact and load controls.
- [x] Add optional raylib-only Make targets without coupling graphics to default CLI builds.
- [x] Capture and visually inspect all scenes, run the core and CLI checks, independently review integration.
- [x] Open the requested viewer and document its controls.

## Verification

- `mingw32-make -j4 test`: 20 core groups, seven export groups, CLI parser and all CLI scenarios passed; no graphics launched by this target.
- `mingw32-make test-viewer`: actual graphics captures passed. Crate occupancy 1352 -> 1292 -> 1264, beam components 1 -> 2, fitted sphere 3071 occupied cells / 1518 exterior cube faces.
- Inspected all scene captures and increased initial crate camera distance to keep the entire object above the footer.
- Strict C11 warning/error check of viewer.c passed.
- Independent review found no actionable issues in occupancy faces, picking, edits, loading, display-only component offsets, scene options or Make isolation.
- Started `build/pixphysiccc-viewer.exe --scene sphere` for the user's explicit request to display the scene.
