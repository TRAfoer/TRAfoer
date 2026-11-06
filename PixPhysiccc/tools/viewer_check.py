"""Check actual fracture motion. Default make/test/smoke remain no-window."""
import json
from pathlib import Path
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "build" / ("pixphysiccc-viewer.exe" if sys.platform == "win32" else "pixphysiccc-viewer")


def verify_report(report):
    assert report["crate_initial_voxels"] == report["crate_final_voxels"] == 1352, report
    assert report["crate_initial_components"] == 1 and report["crate_final_components"] > 1, report
    assert report["crate_max_displacement"] > 0.3, report
    assert report["crate_max_rotation"] > 0.03, report
    assert report["beam_initial_components"] == 1 and report["beam_final_components"] > 1, report
    assert report["beam_drop"] > 0.3, report
    assert report["invalid_states"] == 0 and report["screenshots_written"] == 4, report


def main():
    if not EXE.is_file():
        raise SystemExit("FAIL: physical voxel viewer is not built; run make viewer")
    subprocess.run([str(EXE), "--capture"], cwd=ROOT, check=True, timeout=40)
    report = json.loads((ROOT / "build/viewer_report.json").read_text(encoding="utf-8"))
    verify_report(report)
    screenshots = set()
    for name in ("viewer_crate", "viewer_blast", "viewer_beam", "viewer_settled"):
        path = ROOT / "build" / f"{name}.png"
        data = path.read_bytes()
        assert data[:8] == b"\x89PNG\r\n\x1a\n" and len(data) > 10000, path
        width, height = struct.unpack_from(">II", data, 16)
        assert width >= 640 and height >= 480, path
        screenshots.add(data)
    assert len(screenshots) == 4, "capture must contain four different scene states"
    print("PASS: conserved voxel material, actual fragment translation/rotation, beam free fall, physical scene captures")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
