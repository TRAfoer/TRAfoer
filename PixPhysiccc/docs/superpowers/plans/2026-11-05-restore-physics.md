# Restore Voxel Fracture Physics

User correction: the requested project is force analysis and physical fracture, with cubic voxel appearance. Excavation was not requested. Restore actual motion rather than static geometry or exploded-view presentation.

## Required result

- Intact wooden crate and straight supported beam, represented by touching cubic voxels.
- Explosion/impact breaks structural bonds and transfers physical impulses. All material voxels remain as connected rigid fragments; no erase/carve interaction.
- Unsupported fragments translate, rotate, fall, collide and settle in Jolt. Supported beam sections remain fixed until their connections fail.
- Use a reusable C simulation module over the existing C Jolt bridge, keeping rendering separate.
- Remove the unrequested digging CLI/control paths, pre-cut showcase and presentation-only offsets.
- Default CLI/test remain no-window; explicit view/test-viewer launch the real physical scene.

## Tasks

- [ ] Add a tested simulation lifecycle, compound body mapping, inherited split poses/velocities, explosion and beam loading.
- [ ] Connect the viewer to real body transforms and physical time stepping; restore two original scenes and remove excavation controls.
- [ ] Remove carving from the public CLI and update its integration checks.
- [ ] Reconnect Make targets and test actual movement, rotation, material conservation, gravity and settling.
- [ ] Independently review, inspect real captures, update documentation and open the corrected scene.
