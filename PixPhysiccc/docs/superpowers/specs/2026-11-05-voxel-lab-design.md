# Minimal voxel fracture lab

Approved direction: C application, Makefile, a minimal interactive 3D scene, integer bond strength/damage, no elastic bending. Download the required dependencies and deliver a runnable local prototype.

Two scenes share a six-neighbour voxel bond graph: an exploding hollow wooden crate and a straight cantilever beam that breaks under an adjustable end load. Breaking bonds produces connected components; each component is represented by one rigid body with a compound box collider. Static anchored components stay attached. Use raylib for rendering and Jolt behind a small C ABI for rigid body collision and motion. All application and fracture code is C11; only the physics adapter and upstream Jolt are C++17.

Keep the view simple: one object, a floor, orbit camera, compact controls and live voxel/bond/fragment counts. Fixed-step simulation, reset, pause, scene switch, explosion, beam load controls, and optional bond/strength overlay. No assets, editor, ECS, FEM, or deformable mesh system.

Use pinned source dependencies under ignored vendor/. GNU Make is the project build entry point, including dependency preparation, tests, run and clean. Do not change files outside the user's PixPhysiccc folder or disturb the parent repository's existing changes.

Physics is a game approximation: static beam loads use integer force and lever arms; explosions use separate bond damage and rigid-body impulse. Do not repeatedly subtract static load as damage. Preserve component mass and inherited velocities when splitting.
