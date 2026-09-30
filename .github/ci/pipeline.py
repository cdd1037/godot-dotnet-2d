"""Windows CI orchestration. Run from the repository root; never publishes remotely."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LOGS = ROOT / "bin/ci-logs"
SMOKE = ROOT / "bin/ci-smoke"
ARTIFACTS = ROOT / "bin/ci-artifacts"
EDITOR = ROOT / "bin/godot.windows.editor.x86_64.mono.exe"
TEMPLATE = ROOT / "bin/godot.windows.template_release.x86_64.minimal_extra.mono.exe"
SDK_VERSION = "8.0.425"
MARKER = "CI_DOTNET_SMOKE_OK"


def run(args: list[str | Path], name: str, timeout: int = 7200, marker: str = "") -> str:
    LOGS.mkdir(parents=True, exist_ok=True)
    command = [str(arg) for arg in args]
    print("Running:", subprocess.list2cmdline(command), flush=True)
    with (LOGS / f"{name}.log").open("w", encoding="utf-8") as log:
        log.write("Command: " + json.dumps(command) + "\n")
        log.flush()
        # Direct execution, no shell interpolation; timeout is a failure, never a smoke pass.
        result = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, timeout=timeout)
    output = (LOGS / f"{name}.log").read_text(encoding="utf-8", errors="replace")
    print(output[-12000:], flush=True)
    if result.returncode or (marker and (marker not in output.splitlines() or "ERROR:" in output)):
        raise RuntimeError(f"{name} failed (exit {result.returncode}, required marker {marker!r}); see {LOGS}")
    return output


def sha256(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def verify_ltcg(log: str) -> None:
    # Cache hits can omit compile lines. The fresh forced-link log must still show real LTCG.
    if not re.search(r'/LTCG(?:\s|:|["\]])', log, re.I):
        raise RuntimeError("MSVC /LTCG link flag missing")
    if "Generating code" not in log or "Finished generating code" not in log:
        raise RuntimeError("MSVC whole-program code generation evidence missing")
    if "-fno-use-linker-plugin" in log:
        raise RuntimeError("Unexpected MinGW workaround in MSVC build")


def prepare() -> None:
    if sys.platform != "win32":
        raise RuntimeError("CI production builds must run on native Windows/MSVC")
    image = os.environ.get("ImageVersion") or "uncached-" + os.environ.get("GITHUB_RUN_ID", "local")
    image = re.sub(r"[^A-Za-z0-9_.-]", "_", image)
    if os.environ.get("GITHUB_OUTPUT"):
        with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as output:
            output.write(f"image={image}\n")
    # Override the upstream latestMajor selector only in this disposable CI checkout.
    sdk = json.dumps({"sdk": {"version": SDK_VERSION, "rollForward": "disable"}}, indent=2) + "\n"
    (ROOT / "global.json").write_text(sdk, encoding="utf-8")
    (ROOT / "modules/mono/global.json").write_text(sdk, encoding="utf-8")
    run(["dotnet", "--info"], "dotnet-info", 60)
    run([sys.executable, ".github/ci/install_tool.py", "accesskit"], "accesskit-install", 300)
    run([sys.executable, "-m", "SCons", "--version"], "scons-version", 60)


def scons(target: str) -> list[str]:
    # SCons creates each cache atomically inside its parent after a cache miss.
    (ROOT / ".scons-cache").mkdir(parents=True, exist_ok=True)
    return [
        sys.executable,
        "-m",
        "SCons",
        "platform=windows",
        f"target={target}",
        "arch=x86_64",
        "use_mingw=no",
        "use_llvm=no",
        "msvc_version=14.3",
        "module_mono_enabled=yes",
        "dev_build=no",
        "debug_symbols=no",
        "production=no",
        "verbose=yes",
        "progress=no",
        "silence_msvc=no",
        "accesskit_sdk_path=bin/build_deps/accesskit",
        "--cache-show",
        f"-j{min(4, os.cpu_count() or 2)}",
        "cache_limit=2",
    ]


def editor() -> None:
    run(scons("editor") + ["optimize=speed", "lto=none", "cache_path=.scons-cache/editor"], "build-editor")
    shutil.copy2(ROOT / ".scons_env.json", LOGS / "editor-scons-env.json")
    run([EDITOR, "--headless", "--generate-mono-glue", "modules/mono/glue"], "generate-glue", 300)
    run(
        [
            sys.executable,
            "modules/mono/build_scripts/build_assemblies.py",
            "--godot-output-dir=bin",
            "--godot-platform=windows",
        ],
        "build-managed",
        1800,
    )


def prepare_smoke() -> None:
    SMOKE.mkdir(parents=True, exist_ok=True)
    for source in (ROOT / ".github/ci/smoke").iterdir():
        shutil.copy2(source, SMOKE / source.name)
    version = ET.parse(ROOT / "modules/mono/SdkPackageVersions.props").findtext(".//PackageVersion_Godot_NET_Sdk")
    if not version or not re.fullmatch(r"[A-Za-z0-9.+-]+", version):
        raise RuntimeError("Generated fork SDK version missing or invalid")
    (SMOKE / "CiSmoke.csproj").write_text(
        f'<Project Sdk="Godot.NET.Sdk/{version}"><PropertyGroup><TargetFramework>net8.0</TargetFramework>'
        "<EnableDynamicLoading>true</EnableDynamicLoading></PropertyGroup></Project>\n",
        encoding="utf-8",
    )
    config = ET.Element("configuration")
    sources = ET.SubElement(config, "packageSources")
    ET.SubElement(sources, "clear")
    ET.SubElement(sources, "add", key="fork", value=str(ROOT / "bin/GodotSharp/Tools/nupkgs"))
    ET.SubElement(sources, "add", key="nuget", value="https://api.nuget.org/v3/index.json")
    mappings = ET.SubElement(config, "packageSourceMapping")
    fork = ET.SubElement(mappings, "packageSource", key="fork")
    for pattern in ["Godot.NET.Sdk", "GodotSharp", "GodotSharpEditor", "Godot.SourceGenerators"]:
        ET.SubElement(fork, "package", pattern=pattern)
    upstream = ET.SubElement(mappings, "packageSource", key="nuget")
    ET.SubElement(upstream, "package", pattern="*")
    ET.ElementTree(config).write(SMOKE / "NuGet.Config", encoding="utf-8", xml_declaration=True)
    # The export plugin requires a solution even though `dotnet build` accepts
    # the project alone. Recreate it for deterministic, repeatable CI preparation.
    run(["dotnet", "new", "sln", "--name", "CiSmoke", "--output", SMOKE, "--force"], "smoke-solution", 60)
    run(["dotnet", "sln", SMOKE / "CiSmoke.sln", "add", SMOKE / "CiSmoke.csproj"], "smoke-solution-add", 60)


def test() -> None:
    run(
        [
            "dotnet",
            "test",
            "modules/mono/editor/Godot.NET.Sdk/Godot.SourceGenerators.Tests",
            "--configuration",
            "Release",
            "--logger",
            "trx",
            "--results-directory",
            LOGS / "generator-tests",
            "-m:1",
            "-p:BuildInParallel=false",
        ],
        "source-generators",
        1200,
    )
    prepare_smoke()
    run(["dotnet", "build", SMOKE / "CiSmoke.csproj", "-m:1", "-p:BuildInParallel=false"], "smoke-build", 600)
    run([EDITOR, "--headless", "--editor", "--path", SMOKE, "--import"], "smoke-import", 180)
    run([EDITOR, "--headless", "--path", SMOKE, "--quit-after", "600"], "smoke-editor", 120, MARKER)


def template() -> None:
    args = scons("template_release") + [
        "profile=misc/build_profiles/windows_release_minimal_extra.py",
        "lto=full",
        "cache_path=.scons-cache/template",
    ]
    build_log = run(args, "build-template")
    log = build_log
    if "Finished generating code" not in log:
        # Relink only when an existing local output skipped the linker. Executables
        # are NoCache in this fork, so a fresh Actions checkout normally links once.
        TEMPLATE.unlink(missing_ok=True)
        log = run(args, "link-template-ltcg")
    verify_ltcg(log)
    environment = json.loads((ROOT / ".scons_env.json").read_text(encoding="utf-8"))
    if "/GL" not in environment["CCFLAGS"] or not any("/LTCG" in flag for flag in environment["LINKFLAGS"]):
        raise RuntimeError("Effective MSVC /GL and /LTCG flags missing")
    shutil.copy2(ROOT / ".scons_env.json", LOGS / "template-scons-env.json")
    (LOGS / "lto-verification.json").write_text(
        json.dumps(
            {
                "compiler": "MSVC 14.3",
                "lto": "full",
                "compile": "/GL",
                "link": "/LTCG",
                "code_generation": "Generating code / Finished generating code",
                "sha256": sha256(TEMPLATE),
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )


def export() -> None:
    # Reuse exactly the tested fork SDK feed; never install an upstream Godot package.
    destination = ROOT / "bin/ci-export/CiSmoke.exe"
    destination.parent.mkdir(parents=True, exist_ok=True)
    (SMOKE / "export_presets.cfg").write_text(
        '[preset.0]\nname="CI Windows"\nplatform="Windows Desktop"\nrunnable=true\n'
        'export_filter="all_resources"\nscript_export_mode=2\n\n[preset.0.options]\n'
        f'custom_template/release="{TEMPLATE.as_posix()}"\n'
        'binary_format/architecture="x86_64"\nbinary_format/embed_pck=false\n'
        "dotnet/publish_mode=0\ndotnet/include_debug_symbols=false\n"
        "dotnet/embed_build_outputs=false\n",
        encoding="utf-8",
    )
    run([EDITOR, "--headless", "--path", SMOKE, "--export-release", "CI Windows", destination], "smoke-export", 900)
    run(
        [destination, "--headless", "--quit-after", "600", "--", "--minimal-template"],
        "smoke-export-runtime",
        120,
        MARKER,
    )


def archive(name: str, files: list[tuple[Path, str]]) -> Path:
    target = ARTIFACTS / f"{name}.zip"
    with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED) as output:
        for source, destination in sorted(files):
            if not source.is_file():
                raise FileNotFoundError(source)
            output.write(source, destination)
        for filename in ["LICENSE.txt", "COPYRIGHT.txt", "AUTHORS.md"]:
            output.write(ROOT / filename, filename)
    return target


def package() -> None:
    ARTIFACTS.mkdir(parents=True, exist_ok=True)
    for name in ["smoke-editor", "smoke-export-runtime"]:
        if MARKER not in (LOGS / f"{name}.log").read_text(encoding="utf-8"):
            raise RuntimeError(f"Missing successful {name}")
    lto = json.loads((LOGS / "lto-verification.json").read_text())
    if lto["sha256"] != sha256(TEMPLATE):
        raise RuntimeError("Template changed since LTO verification")
    editor_files = [(p, p.name) for p in (ROOT / "bin").glob("godot.windows.editor.x86_64.mono*.exe")]
    editor_files += [
        (p, p.relative_to(ROOT / "bin").as_posix())
        for p in (ROOT / "bin/GodotSharp").rglob("*")
        if p.is_file() and "nupkgs" not in p.parts
    ]
    packages = [(p, p.name) for p in (ROOT / "bin/GodotSharp/Tools/nupkgs").glob("*.nupkg")]
    if not editor_files or len(packages) < 4:
        raise RuntimeError("Editor or fork NuGet packages missing")
    template_files = [
        (p, p.name) for p in (ROOT / "bin").glob("godot.windows.template_release.x86_64.minimal_extra.mono*.exe")
    ]
    template_files += [(ROOT / "misc/build_profiles/windows_release_minimal_extra.py", "profile.py")]
    outputs = [
        archive("godot-windows-x64-editor-dotnet", editor_files),
        archive("godot-windows-x64-fork-nuget", packages),
        archive("godot-windows-x64-release-template-extra-lto", template_files),
    ]
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    manifest = {
        "repository": os.environ.get("GITHUB_REPOSITORY", ""),
        "commit": commit,
        "compiler": "MSVC 14.3 (windows-2022 serviced toolset; see build log)",
        "runner_image": os.environ.get("ImageVersion", "unknown"),
        "dotnet_sdk": SDK_VERSION,
        "nuget_version": ET.parse(ROOT / "modules/mono/SdkPackageVersions.props").findtext(
            ".//PackageVersion_Godot_NET_Sdk"
        ),
        "template_profile": "windows_release_minimal_extra.py",
        "template_lto": lto,
        "runtime_test": "native Windows headless C# untrimmed JIT export",
        "limitations": [
            "No Vulkan/display test",
            "No Windows trimmed-JIT or NativeAOT runtime test",
            "Editor API is a superset; excluded template modules remain unavailable",
        ],
        "files": {p.name: {"bytes": p.stat().st_size, "sha256": sha256(p)} for p in outputs},
    }
    (ARTIFACTS / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    outputs.append(ARTIFACTS / "manifest.json")
    (ARTIFACTS / "SHA256SUMS.txt").write_text("".join(f"{sha256(p)}  {p.name}\n" for p in outputs), encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stage", choices=["prepare", "editor", "test", "template", "export", "package"])
    args = parser.parse_args()
    os.chdir(ROOT)
    globals()[args.stage]()


if __name__ == "__main__":
    main()
