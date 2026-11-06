#!/usr/bin/env python3
"""Small shell-independent filesystem helpers for GNU Make."""
from __future__ import annotations

import pathlib
import re
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"


def write_if_changed(path: pathlib.Path, contents: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_text(encoding="utf-8") != contents:
        path.write_text(contents, encoding="utf-8", newline="\n")


def sources() -> None:
    # Match the upstream source list with ENABLE_OBJECT_STREAM=OFF. Keeping this
    # list derived from upstream avoids maintaining a second physics manifest.
    manifest = (ROOT / "vendor/JoltPhysics/Jolt/Jolt.cmake").read_text(encoding="utf-8")
    manifest = manifest.split("if (ENABLE_OBJECT_STREAM)", 1)[0]
    relative = re.findall(r"\$\{JOLT_PHYSICS_ROOT\}/([^\s)]+\.cpp)", manifest)
    if not relative:
        raise RuntimeError("No Jolt sources found in pinned upstream source manifest")
    jolt_sources = [f"vendor/JoltPhysics/Jolt/{name}" for name in relative]
    write_if_changed(BUILD / "sources.mk", "JOLT_SOURCES := " + " \\\n  ".join(jolt_sources) + "\n")
    objects = [f"build/jolt/{pathlib.PurePosixPath(name).with_suffix('.o')}" for name in relative]
    write_if_changed(BUILD / "jolt-objects.rsp", "\n".join(objects) + "\n")


def main() -> None:
    action, *arguments = sys.argv[1:]
    if action == "sources":
        sources()
    elif action == "mkdir":
        for value in arguments:
            (ROOT / value).mkdir(parents=True, exist_ok=True)
    elif action == "clean":
        # Only the fixed project-local output directory may be removed.
        target = BUILD.resolve()
        if target.parent != ROOT.resolve() or target.name != "build":
            raise RuntimeError("Unsafe build directory")
        if target.exists():
            shutil.rmtree(target)
    elif action == "run":
        raise SystemExit(subprocess.run([str((ROOT / arguments[0]).resolve()),
                                         *arguments[1:]], cwd=ROOT).returncode)
    else:
        raise RuntimeError(f"Unknown build helper action: {action}")


if __name__ == "__main__":
    main()
