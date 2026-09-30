"""Create an immutable-input tag's draft release; never publish or move a tag."""

from __future__ import annotations

import hashlib
import json
import os
import re
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path


def validate_tag(tag: str) -> None:
    if not re.fullmatch(r"v\d+\.\d+\.\d+-custom\.\d+", tag):
        raise ValueError("Expected a tag such as v4.7.2-custom.1")


def verify_assets(directory: Path, repository: str, commit: str) -> list[Path]:
    manifest = json.loads((directory / "manifest.json").read_text(encoding="utf-8"))
    if manifest["commit"] != commit or manifest["repository"] != repository:
        raise ValueError("Artifact provenance does not match this repository and tag commit")
    files = []
    for line in (directory / "SHA256SUMS.txt").read_text().splitlines():
        digest, name = line.split("  ", 1)
        if not re.fullmatch(r"[a-zA-Z0-9_.-]+", name) or not re.fullmatch(r"[0-9a-f]{64}", digest):
            raise ValueError("Invalid checksum entry")
        path = directory / name
        if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            raise ValueError(f"Checksum mismatch: {name}")
        files.append(path)
    for name, info in manifest["files"].items():
        path = directory / name
        if path.stat().st_size != info["bytes"] or hashlib.sha256(path.read_bytes()).hexdigest() != info["sha256"]:
            raise ValueError(f"Manifest digest or size mismatch: {name}")
    expected = set(manifest["files"]) | {"manifest.json"}
    if {path.name for path in files} != expected or len(files) != len(expected):
        raise ValueError("Checksum file does not cover exactly the build outputs and manifest")
    if len(manifest["files"]) != 3:
        raise ValueError("Expected exactly editor, NuGet, and LTO template archives")
    return files + [directory / "SHA256SUMS.txt"]


def validate_existing_release(release: dict, commit: str) -> None:
    if not release["draft"]:
        raise RuntimeError("Refusing to modify an already published release")
    marker = "<!-- ci-source-sha: " + commit + " -->"
    if release.get("target_commitish") != commit or marker not in (release.get("body") or ""):
        raise RuntimeError("Existing draft has different or unverified source provenance")


def main() -> None:
    if os.environ.get("GITHUB_EVENT_NAME") != "push" or os.environ.get("GITHUB_REF_TYPE") != "tag":
        raise RuntimeError("Release drafts are allowed only for tag-push runs")
    tag = os.environ["GITHUB_REF_NAME"]
    validate_tag(tag)
    repository, commit = os.environ["GITHUB_REPOSITORY"], os.environ["GITHUB_SHA"]
    files = verify_assets(Path("bin/ci-artifacts"), repository, commit)
    token = os.environ["GH_TOKEN"]
    api = "https://api.github.com/repos/" + repository

    def request(url, method="GET", data=None, binary=False):
        headers = {
            "Authorization": "Bearer " + token,
            "Accept": "application/vnd.github+json",
            "X-GitHub-Api-Version": "2022-11-28",
            "User-Agent": "godot-fork-draft-ci",
        }
        if data is not None:
            headers["Content-Type"] = "application/octet-stream" if binary else "application/json"
            if not binary:
                data = json.dumps(data).encode()
        with urllib.request.urlopen(
            urllib.request.Request(url, data=data, headers=headers, method=method), timeout=120
        ) as response:
            return json.load(response)

    ref = request(api + "/git/ref/tags/" + urllib.parse.quote(tag, safe=""))["object"]
    # Support lightweight and annotated tags without assuming target_commitish is enough.
    for _ in range(10):
        if ref["type"] == "commit":
            break
        if ref["type"] != "tag":
            raise RuntimeError("Tag does not resolve to a commit")
        ref = request(api + "/git/tags/" + ref["sha"])["object"]
    if ref["type"] != "commit" or ref["sha"] != commit:
        raise RuntimeError("Tag moved or does not match the tested commit")
    marker = "<!-- ci-source-sha: " + commit + " -->"
    try:
        release = request(api + "/releases/tags/" + urllib.parse.quote(tag, safe=""))
    except urllib.error.HTTPError as error:
        if error.code != 404:
            raise
        release = None
    if release is not None:
        validate_existing_release(release, commit)
    else:
        manifest = json.loads(Path("bin/ci-artifacts/manifest.json").read_text())
        body = (
            f"{marker}\n\nWindows x64 custom 2D .NET build from `{commit}`.\n\n"
            "- Editor plus its generated GodotSharp assemblies\n"
            "- Matching fork NuGet packages (download only, not published to NuGet.org)\n"
            "- Final minimal-extra Release template with MSVC full LTO\n"
            "- SHA256SUMS.txt and manifest.json for provenance and checksums\n\n"
            f".NET SDK: {manifest['dotnet_sdk']}; fork NuGet version: {manifest['nuget_version']}. "
            "The tag does not automatically change NuGet package versions.\n\n"
            "Checks: source generator tests and native Windows headless untrimmed-JIT C# export smoke. "
            "Vulkan/display, Windows trimmed-JIT and NativeAOT are not certified by this workflow. "
            "The editor API is broader than the minimal template; excluded modules remain unavailable.\n\n"
            "Review these artifacts and notes before publishing this draft.\n"
        )
        release = request(
            api + "/releases",
            "POST",
            {"tag_name": tag, "target_commitish": commit, "name": tag, "body": body, "draft": True, "prerelease": True},
        )
    assets = {asset["name"]: asset for asset in release.get("assets", [])}
    # Idempotent retry: skip byte-identical assets; never silently replace a different one.
    for path in files:
        data = path.read_bytes()
        digest = "sha256:" + hashlib.sha256(data).hexdigest()
        if path.name in assets:
            if assets[path.name].get("digest") != digest:
                raise RuntimeError(f"Existing draft asset differs or has no verified digest: {path.name}")
            continue
        upload = release["upload_url"].split("{", 1)[0] + "?name=" + urllib.parse.quote(path.name, safe="")
        request(upload, "POST", data, binary=True)
    print("Draft ready for manual review:", release["html_url"])


if __name__ == "__main__":
    main()
