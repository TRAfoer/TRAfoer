#!/usr/bin/env python3
"""Fetch and verify the exact source revisions used by the Makefile."""
from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[1]
DEPENDENCIES = {
    "raylib": ("https://github.com/raysan5/raylib.git", "5.5",
               "c1ab645ca298a2801097931d1079b10ff7eb9df8", "src", "LICENSE",
               "be3443c3fb97f7cbd18ae3f0ea2a73e6c1486128df33b639dce7e44070fc285d"),
    "JoltPhysics": ("https://github.com/jrouwe/JoltPhysics.git", "v5.3.0",
                    "0373ec0dd762e4bc2f6acdb08371ee84fa23c6db", "Jolt", "LICENSE",
                    "834644d7e9dffee9be0220748015667649d3361611b01c7e6839a5eb58e89711"),
}


def git(*arguments: str, cwd: pathlib.Path | None = None) -> str:
    result = subprocess.run(["git", *arguments], cwd=cwd, check=True,
                            text=True, stdout=subprocess.PIPE)
    return result.stdout.strip()


def digest(path: pathlib.Path) -> str:
    with path.open("rb") as handle:
        return hashlib.file_digest(handle, "sha256").hexdigest()


def fetch_archive(name: str, destination: pathlib.Path) -> None:
    url, tag, revision, source, _, archive_hash = DEPENDENCIES[name]
    repository = url.removeprefix("https://github.com/").removesuffix(".git")
    archive_url = f"https://codeload.github.com/{repository}/tar.gz/{revision}"
    cache = ROOT / ".cache"
    cache.mkdir(exist_ok=True)
    archive = cache / f"{name}-{revision}.tar.gz"
    if not archive.exists() or digest(archive) != archive_hash:
        print(f"Fetching {name} {tag} ({revision})", flush=True)
        partial = archive.with_suffix(".part")
        with urllib.request.urlopen(archive_url, timeout=60) as response, partial.open("wb") as output:
            shutil.copyfileobj(response, output)
        if digest(partial) != archive_hash:
            raise RuntimeError(f"{name}: source archive SHA-256 does not match its pin")
        partial.replace(archive)
    with tempfile.TemporaryDirectory(prefix=f".{name}-", dir=ROOT / "vendor") as temporary:
        stage = pathlib.Path(temporary) / "source"
        stage.mkdir()
        hashes = {}
        with tarfile.open(archive, "r:gz") as bundle:
            for member in bundle:
                parts = pathlib.PurePosixPath(member.name).parts
                if len(parts) < 2:
                    continue
                relative = pathlib.PurePosixPath(*parts[1:])
                if relative.is_absolute() or ".." in relative.parts:
                    raise RuntimeError(f"Unsafe archive path: {member.name}")
                # Preserve source and root documents/licenses, omitting assets.
                if relative.parts[0] != source and len(relative.parts) != 1:
                    continue
                if not member.isfile():
                    continue
                target = stage / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                with bundle.extractfile(member) as input_file, target.open("wb") as output:
                    shutil.copyfileobj(input_file, output)
                hashes[relative.as_posix()] = digest(target)
        marker = {"revision": revision, "archive_sha256": archive_hash, "files": hashes}
        (stage / ".pixphysiccc-source.json").write_text(json.dumps(marker, indent=2), encoding="utf-8")
        if stage.resolve().parent.parent != (ROOT / "vendor").resolve() or destination.parent != ROOT / "vendor":
            raise RuntimeError("Unsafe dependency destination")
        stage.replace(destination)


def ensure_dependency(name: str) -> None:
    _, _, revision, source, license_name, archive_hash = DEPENDENCIES[name]
    destination = ROOT / "vendor" / name
    if not destination.exists():
        fetch_archive(name, destination)
    if (destination / ".git").is_dir():
        actual = git("rev-parse", "HEAD", cwd=destination)
        if actual != revision:
            raise RuntimeError(f"{name}: expected {revision}, found {actual}; refusing to overwrite it")
        subprocess.run(["git", "diff", "--quiet", "HEAD", "--", source, license_name],
                       cwd=destination, check=True)
    elif (destination / ".pixphysiccc-source.json").is_file():
        marker = json.loads((destination / ".pixphysiccc-source.json").read_text(encoding="utf-8"))
        if marker.get("revision") != revision or marker.get("archive_sha256") != archive_hash:
            raise RuntimeError(f"{name}: source revision metadata does not match its pin")
        for relative, expected in marker["files"].items():
            if digest(destination / relative) != expected:
                raise RuntimeError(f"{name}: upstream file was modified: {relative}")
    else:
        raise RuntimeError(f"{destination} exists but has no verified source metadata")
    if not (destination / source).is_dir():
        raise RuntimeError(f"{name}: upstream source is missing")
    if not (destination / license_name).is_file():
        raise RuntimeError(f"{name}: upstream license is missing")
    print(f"Verified {name}: {revision}", flush=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("names", nargs="*", choices=tuple(DEPENDENCIES))
    args = parser.parse_args()
    (ROOT / "vendor").mkdir(exist_ok=True)
    try:
        for name in args.names or DEPENDENCIES:
            ensure_dependency(name)
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError) as error:
        print(f"Dependency preparation failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
