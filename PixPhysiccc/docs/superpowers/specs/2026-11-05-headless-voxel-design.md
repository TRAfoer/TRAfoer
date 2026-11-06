# Headless voxel analysis

User steering: emphasize voxels as editable material, and work without UI or popup windows. Subsequent presentation must use cubes to fit and pixelate shapes, rather than render original models. Default builds, runs and checks must never initialize a graphics context. Preserve the old viewer as reference source outside the build; never launch it during this work.

Move the main result to a voxel occupancy/integrity field. An impact damages individual occupied cells and bonds; cells can disappear, leaving holes and newly exposed faces. Recompute connected components from surviving occupied cells. Remove artificial 2x2 strong tile seams. Beam loading uses the remaining cells and remaining section connections, so carving a notch changes capacity.

Default command line executable is C11 with no raylib or Jolt dependency. It runs crate and beam analysis and exports machine-readable occupancy, damage, component labels and loads, plus native cubic MagicaVoxel .vox data before and after. Occupied cells are the source for a future block renderer; there is no original smooth model or OBJ rendering path. It performs no falling-body animation. The independently usable Jolt adapter remains an optional backend; the former viewer is historical reference source.

Coordinates identify stable cell slots; occupied=0 means removed, component=-1. Cell count is allocated slots; active voxel counts are reported separately. Damage and load remain integer game units. Internal graph bonds may be broken while both cells survive; this must not be confused with removing material. Six-neighbour occupied adjacency defines exposed geometry; live bonds define structural connected components.

CLI supports crate/beam, object size, ordered repeated radial impact/carve actions, end load, output directory, and no default action. Default crate side16 receives a local surface impact; default beam length16 receives an overload. Reports and surface exports make these behaviors inspectable without opening an application.
