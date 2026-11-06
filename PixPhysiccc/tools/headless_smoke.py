"""Verify CLI material/bond analysis and exports without starting a renderer."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import uuid

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "build" / ("pixphysiccc.exe" if sys.platform == "win32" else "pixphysiccc")
CHECKS = ROOT / "build" / "headless_checks"
OFFSETS = ((1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1), (0, 0, -1))


def command(*args):
    return subprocess.run([str(EXE), *args], cwd=ROOT, text=True,
                          capture_output=True, timeout=15)


def vox_coordinates(path):
    raw = path.read_bytes()
    assert raw[:4] == b"VOX " and struct.unpack_from("<I", raw, 4)[0] == 150, path
    name, content, children = struct.unpack_from("<4sII", raw, 8)
    assert name == b"MAIN" and content == 0 and 20 + children == len(raw), path
    result = None
    offset = 20
    while offset < len(raw):
        name, size, child_size = struct.unpack_from("<4sII", raw, offset)
        start = offset + 12
        assert start + size + child_size <= len(raw), path
        if name == b"XYZI":
            count = struct.unpack_from("<I", raw, start)[0]
            assert size == 4 + 4*count, path
            entries = [struct.unpack_from("4B", raw, start+4+4*i) for i in range(count)]
            assert all(color > 0 for _, _, _, color in entries), path
            result = {(x, y, z) for x, y, z, _ in entries}
            assert len(result) == count, path
        offset = start + size + child_size
    assert offset == len(raw) and result is not None, path
    return result


def verify_snapshot(directory, name):
    state = json.loads((directory / f"{name}.json").read_text(encoding="utf-8"))
    assert state["format"] == "pixphysiccc.voxels.v1"
    cells = state["voxels"]
    active = {cell["id"]: cell for cell in cells if cell["occupied"]}
    coordinates = {(cell["x"], cell["y"], cell["z"]) for cell in active.values()}
    assert len(cells) == state["allocated_voxels"]
    assert len(active) == len(coordinates) == state["occupied_voxels"]
    assert len({cell["id"] for cell in cells}) == len(cells)
    assert all(0 < cell["integrity"] <= 100 and cell["component"] >= 0 for cell in active.values())
    assert all(cell["integrity"] == 0 and cell["component"] == -1 for cell in cells if not cell["occupied"])
    exposed = sum((x+dx, y+dy, z+dz) not in coordinates
                  for x, y, z in coordinates for dx, dy, dz in OFFSETS)
    assert exposed == state["surface"]["exposed_faces"]
    assert exposed + 2*state["surface"]["shared_faces"] == 6*len(active)
    assert vox_coordinates(directory / f"{name}.vox") == coordinates

    adjacency = {key: [] for key in active}
    live_bonds = 0
    for bond in state["bonds"]:
        intact = bond["a"] in active and bond["b"] in active and bond["damage"] < bond["strength"]
        assert bond["intact"] == intact
        if intact:
            live_bonds += 1
            adjacency[bond["a"]].append(bond["b"])
            adjacency[bond["b"]].append(bond["a"])
    unseen = set(active)
    labels = set()
    while unseen:
        first = unseen.pop()
        label = active[first]["component"]
        assert label not in labels
        labels.add(label)
        queue = [first]
        for cell in queue:
            assert active[cell]["component"] == label
            for neighbor in adjacency[cell]:
                if neighbor in unseen:
                    unseen.remove(neighbor)
                    queue.append(neighbor)
    assert len(labels) == state["components"]
    return state, live_bonds


def case(name, *args):
    output = CHECKS / name
    result = command(*args, "--output", str(output))
    assert result.returncode == 0, (name, result.stdout, result.stderr)
    assert "raylib" not in result.stdout.lower() and "GLFW" not in result.stdout
    report = json.loads((output / "report.json").read_text(encoding="utf-8"))
    assert report["headless"] is True and report["representation"] == "voxel_cubes"
    before, initial_bonds = verify_snapshot(output, "before")
    after, final_bonds = verify_snapshot(output, "after")
    for key, state, bonds in (("initial", before, initial_bonds), ("final", after, final_bonds)):
        for field in ("occupied_voxels", "components"):
            assert report[key][field] == state[field], (name, field)
        assert report[key]["live_bonds"] == bonds
        assert report[key]["exposed_faces"] == state["surface"]["exposed_faces"]
    assert report["total_removed_voxels"] == before["occupied_voxels"] - after["occupied_voxels"]
    assert report["total_removed_voxels"] == 0
    assert all(action["removed_voxels"] == 0 for action in report["actions"])
    material = lambda state: {(cell["id"], cell["x"], cell["y"], cell["z"],
                               cell["material"], cell["occupied"], cell["integrity"])
                              for cell in state["voxels"]}
    assert material(before) == material(after), name
    assert report["total_broken_bonds"] == initial_bonds - final_bonds
    print(f"PASS {name}: voxels {before['occupied_voxels']} -> {after['occupied_voxels']}, "
          f"components {before['components']} -> {after['components']}")
    return report, before, after


def main():
    # The previous app handles --help without a window. Refuse all other calls
    # until the executable identifies itself as the replacement CLI.
    assert EXE.is_file(), "run make first"
    help_result = command("--help")
    assert help_result.returncode == 0 and "--scene" in help_result.stdout, "headless CLI not built"
    CHECKS.mkdir(parents=True, exist_ok=True)

    report, before, after = case("crate_local")
    assert before["occupied_voxels"] == after["occupied_voxels"] == 1352
    assert report["total_broken_bonds"] > 0
    assert any(0 < bond["damage"] < bond["strength"] for bond in after["bonds"])

    report, _, _ = case("beam_load", "--scene", "beam")
    assert report["final"]["components"] == 2 and report["total_removed_voxels"] == 0

    intact, _, _ = case("beam_intact_60", "--scene", "beam", "--load", "60")
    assert intact["final"]["components"] == 1

    single, _, first = case("single_impact", "--impact", "15,7.5,7.5,3,90")
    assert single["total_broken_bonds"] == 0
    assert any(bond["damage"] > 0 for bond in first["bonds"])
    report, before, after = case("repeated_impacts", "--impact", "15,7.5,7.5,3,90",
                                 "--impact", "15,7.5,7.5,3,90")
    assert [entry["type"] for entry in report["actions"]] == ["impact", "impact"]
    assert report["actions"][0]["broken_bonds"] == 0
    assert report["actions"][1]["broken_bonds"] > 0
    assert before["occupied_voxels"] == after["occupied_voxels"] == 1352
    assert any(second["damage"] > first_bond["damage"]
               for first_bond, second in zip(first["bonds"], after["bonds"]))

    report, _, _ = case("unchanged", "--no-default")
    assert report["initial"] == report["final"] and not report["actions"]

    invalid = (("--unknown",), ("--size", "0"), ("--size", "2x"),
               ("--carve", "1,1,1,1"), ("--carve", "1,1,1,-1"),
               ("--impact", "1,1,1,2,nan"),
               ("--impact", "nan,1,1,2,10"), ("--load", "10"), ("--output",))
    for args in invalid:
        output = CHECKS / ("invalid-" + uuid.uuid4().hex)
        result = command("--output", str(output), *args)
        assert result.returncode == 2 and not output.exists(), (args, result.stdout, result.stderr)
    print(f"PASS {len(invalid)} invalid commands rejected before output creation")
    print("PASS headless material/bond CLI, conserved voxel mass, cumulative damage, stable IDs, JSON and cubic VOX exports")


if __name__ == "__main__":
    main()
