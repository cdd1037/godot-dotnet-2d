"""Fast checks and conservative change classification, including on fork PRs."""

from __future__ import annotations

import ast
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))


def git(*args: str) -> str:
    return subprocess.check_output(["git", *args], cwd=ROOT).decode("utf-8")


def docs_only(paths: list[str]) -> bool:
    return bool(paths) and all(
        path.endswith((".md", ".rst")) or path.startswith(".github/ISSUE_TEMPLATE/") for path in paths
    )


def classify() -> list[str]:
    event_name = os.environ.get("GITHUB_EVENT_NAME", "")
    event_path = os.environ.get("GITHUB_EVENT_PATH")
    event = json.loads(Path(event_path).read_text()) if event_path else {}
    ref = os.environ.get("GITHUB_REF", "")
    title = ((event.get("head_commit") or {}).get("message") or "").partition("\n")[0]
    full = (
        event_name == "workflow_dispatch"
        or ref.startswith("refs/tags/")
        or (event_name == "push" and ref.startswith("refs/heads/") and "[full-ci]" in title)
    )
    base = event.get("pull_request", {}).get("base", {}).get("sha") or event.get("before", "")
    if base and set(base) != {"0"}:
        try:
            paths = git("diff", "--name-only", "-z", f"{base}...HEAD").strip("\0").split("\0")
        except subprocess.CalledProcessError:
            # A force-push can make the old commit unreachable in a fresh checkout.
            base = ""
    if not base or set(base) == {"0"}:
        # New branches and manual/tag builds get the safe default, not a guessed empty diff.
        paths = git("ls-files", "-z").strip("\0").split("\0")
    if os.environ.get("GITHUB_REF", "").startswith("refs/tags/"):
        from draft_release import validate_tag

        validate_tag(os.environ["GITHUB_REF_NAME"])
    build = full or not docs_only(paths)
    if not base or set(base) == {"0"}:
        # No trusted diff base: still build, but do not turn inherited upstream
        # formatting debt into a first-push blocker. Always check maintained CI files.
        paths = [name for name in paths if name.startswith(".github/ci/")]
    output = os.environ.get("GITHUB_OUTPUT")
    if output:
        with open(output, "a", encoding="utf-8") as stream:
            stream.write(f"build={str(build).lower()}\nfull={str(full).lower()}\n")
    print(f"Native/managed build required: {build}; full LTO artifacts: {full}")
    return paths


def shader_generators() -> None:
    # The upstream runner imports deleted gles3_builders. Exercise all four retained
    # RenderingDevice/raw GLSL fixtures without restoring the removed OpenGL path.
    from glsl_builders import build_raw_header, build_rd_header

    for directory, builder in [("glsl", build_raw_header), ("rd_glsl", build_rd_header)]:
        for name in ["compute", "vertex_fragment"]:
            fixture = ROOT / f"tests/python_build/fixtures/{directory}/{name}"
            with tempfile.TemporaryDirectory() as temporary:
                output = Path(temporary) / f"{name}.out"
                builder(str(output), shader=str(fixture.with_suffix(".glsl")))
                if output.read_bytes() != fixture.with_suffix(".out").read_bytes():
                    raise RuntimeError(f"Shader generator fixture mismatch: {fixture}")
    print("Four retained shader generator fixtures passed")


def main() -> None:
    os.chdir(ROOT)
    changed = classify()
    tracked = git("ls-files", "-z").strip("\0").split("\0")
    for filename in tracked:
        path = ROOT / filename
        if path.is_file() and (path.suffix == ".py" or path.name in ["SConstruct", "SCsub"]):
            ast.parse(path.read_text(encoding="utf-8-sig"), filename=filename)
    for filename in changed:
        path = ROOT / filename
        if (
            not path.is_file()
            or "thirdparty" in path.parts
            or path.suffix in [".svg", ".patch", ".out"]
            or filename.endswith(".test.txt")
        ):
            continue
        data = path.read_bytes()
        if b"\0" in data:
            continue
        try:
            original = data.decode("utf-8-sig")
        except UnicodeDecodeError:
            continue
        if not original:
            continue
        eol = "\r\n" if path.suffix in [".csproj", ".sln", ".bat"] or filename.startswith("misc/msvs") else "\n"
        expected = eol.join(line.rstrip("\r\n\t ") for line in original.splitlines()).rstrip(eol) + eol
        expected_bytes = expected.encode("utf-8")
        if path.suffix in [".csproj", ".sln"]:
            expected_bytes = b"\xef\xbb\xbf" + expected_bytes
        if data != expected_bytes:
            raise RuntimeError(f"File formatting differs from misc/scripts/file_format.py: {filename}")
    python_changes = [
        name
        for name in changed
        if (ROOT / name).is_file()
        and (name.endswith(".py") or Path(name).name in ["SConstruct", "SCsub"])
        and "thirdparty" not in Path(name).parts
    ]
    if python_changes:
        subprocess.run([sys.executable, "-m", "ruff", "check", "--no-fix", "--", *python_changes], check=True)
        subprocess.run([sys.executable, "-m", "ruff", "format", "--check", "--", *python_changes], check=True)
    shader_generators()
    print("Tracked Python/SCons syntax and whitespace checks passed")


if __name__ == "__main__":
    main()
