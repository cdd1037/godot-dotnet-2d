"""Install checksum-pinned official CI tools without changing system settings."""

from __future__ import annotations

import argparse
import hashlib
import io
import subprocess
import sys
import tarfile
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "bin/ci-tools"
ACTIONLINT_VERSION = "1.7.7"
ACTIONLINT_HASHES = {
    "windows": "7f12f1801bca3d480d67aaf7774f4c2a6359a3ca8eebe382c95c10c9704aa731",
    "linux": "023070a287cd8cccd71515fedc843f1985bf96c436b7effaecce67290e7e0757",
}
ACCESSKIT_HASH = "41b30b1382d03b225b6f7e8721269a32557a68065804f3797bdffc34a3273498"


def fetch(url: str, digest: str) -> bytes:
    with urllib.request.urlopen(url, timeout=120) as response:
        data = response.read()
    if hashlib.sha256(data).hexdigest() != digest:
        raise RuntimeError(f"SHA256 mismatch for {url}")
    return data


def actionlint() -> None:
    windows = sys.platform == "win32"
    platform = "windows" if windows else "linux"
    suffix = "zip" if windows else "tar.gz"
    name = f"actionlint_{ACTIONLINT_VERSION}_{platform}_amd64.{suffix}"
    data = fetch(
        f"https://github.com/rhysd/actionlint/releases/download/v{ACTIONLINT_VERSION}/{name}",
        ACTIONLINT_HASHES[platform],
    )
    executable = TOOLS / ("actionlint.exe" if windows else "actionlint")
    TOOLS.mkdir(parents=True, exist_ok=True)
    if windows:
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            executable.write_bytes(archive.read("actionlint.exe"))
    else:
        with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as archive:
            executable.write_bytes(archive.extractfile("actionlint").read())
        executable.chmod(0o755)
    subprocess.run(
        [
            str(executable),
            "-color",
            "-shellcheck=",
            "-pyflakes=",
            *map(str, sorted((ROOT / ".github/workflows").glob("*.yml"))),
        ],
        cwd=ROOT,
        check=True,
    )


def accesskit() -> None:
    version = "0.22.3"
    data = fetch(
        f"https://github.com/godotengine/godot-accesskit-c-static/releases/download/{version}/accesskit-c-{version}.zip",
        ACCESSKIT_HASH,
    )
    destination = ROOT / "bin/build_deps/accesskit"
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        for name in archive.namelist():
            relative = Path(name).relative_to(f"accesskit-c-{version}")
            # Only native MSVC x64 inputs, headers and license; no unused target SDKs.
            if not (
                relative.parts
                and (
                    relative.parts[0] == "include"
                    or relative.as_posix().startswith("lib/windows/x86_64/msvc/static/")
                    or relative.name.lower().startswith("license")
                )
            ):
                continue
            target = (destination / relative).resolve()
            if not target.is_relative_to(destination.resolve()):
                raise RuntimeError("Unsafe archive path")
            if not name.endswith("/"):
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(archive.read(name))
    if not (destination / "include/accesskit.h").is_file():
        raise RuntimeError("AccessKit headers missing")
    if not list((destination / "lib/windows/x86_64/msvc/static").glob("*.lib")):
        raise RuntimeError("AccessKit MSVC library missing")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tool", choices=["actionlint", "accesskit"])
    globals()[parser.parse_args().tool]()
