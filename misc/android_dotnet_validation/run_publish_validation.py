#!/usr/bin/env python3
"""Publish a matched Android Mono pair; inspect IL without claiming device execution.

Requires the matching locally built fork NuGet feed, .NET SDK 10.0.401 and normal
MSBuild task-host IPC. Does not use MAUI, Android workloads, or Gradle.
"""

import argparse
import hashlib
import json
import shutil
import subprocess
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent


def run(command, log, cwd=ROOT):
    with log.open("w", encoding="utf-8") as stream:
        result = subprocess.run([str(arg) for arg in command], cwd=cwd, stdout=stream, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(f"Command failed ({result.returncode}); see {log}")
    return log.read_text(encoding="utf-8-sig")


def prepare(project, version, feed, trimmed):
    shutil.copytree(HERE / "fixture", project)
    (project / "global.json").write_text(
        json.dumps({"sdk": {"version": "10.0.401", "rollForward": "disable"}}) + "\n", encoding="utf-8"
    )
    xml = ET.Element("Project", Sdk=f"Godot.NET.Sdk/{version}")
    props = ET.SubElement(xml, "PropertyGroup")
    for key, value in {
        "TargetFramework": "net10.0",
        "EnableDynamicLoading": "true",
        # Deliberately project-local: proves late SDK evaluation selects GODOT_TRIMMED.
        "PublishTrimmed": str(trimmed).lower(),
    }.items():
        ET.SubElement(props, key).text = value
    ET.ElementTree(xml).write(project / "AndroidMonoSmoke.csproj", encoding="utf-8", xml_declaration=True)
    config = ET.Element("configuration")
    sources = ET.SubElement(config, "packageSources")
    ET.SubElement(sources, "clear")
    ET.SubElement(sources, "add", key="fork", value=str(feed))
    ET.SubElement(sources, "add", key="nuget", value="https://api.nuget.org/v3/index.json")
    mappings = ET.SubElement(config, "packageSourceMapping")
    fork = ET.SubElement(mappings, "packageSource", key="fork")
    for pattern in ["Godot.NET.Sdk", "GodotSharp", "GodotSharpEditor", "Godot.SourceGenerators"]:
        ET.SubElement(fork, "package", pattern=pattern)
    upstream = ET.SubElement(mappings, "packageSource", key="nuget")
    ET.SubElement(upstream, "package", pattern="*")
    ET.ElementTree(config).write(project / "NuGet.Config", encoding="utf-8", xml_declaration=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="New empty evidence directory")
    parser.add_argument("--feed", type=Path, default=ROOT / "bin/GodotSharp/Tools/nupkgs")
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists() and any(output.iterdir()):
        parser.error("Output must be empty; existing results are never erased")
    output.mkdir(parents=True, exist_ok=True)
    version = ET.parse(ROOT / "modules/mono/SdkPackageVersions.props").findtext(".//PackageVersion_Godot_NET_Sdk")
    if not version or not args.feed.is_dir():
        parser.error("Build matching fork packages first")
    actual_sdk = subprocess.check_output(["dotnet", "--version"], cwd=ROOT, text=True).strip()
    if actual_sdk != "10.0.401":
        raise RuntimeError(f"Matched validation requires SDK 10.0.401, got {actual_sdk}")
    inspector = output / "inspector"
    run(
        [
            "dotnet",
            "build",
            HERE / "InspectPayload",
            "-c",
            "Release",
            "-o",
            inspector,
            "-m:1",
            "-p:UseSharedCompilation=false",
            "--disable-build-servers",
        ],
        output / "build-inspector.log",
    )
    reports = {}
    for mode in ("jit", "trimmed-jit"):
        project = output / mode / "project"
        prepare(project, version, args.feed.resolve(), mode == "trimmed-jit")
        csproj = project / "AndroidMonoSmoke.csproj"
        publish = output / mode / "publish"
        run(
            [
                "dotnet",
                "publish",
                csproj,
                "-c",
                "ExportRelease",
                "-r",
                "android-arm64",
                "--self-contained",
                "true",
                "-p:PublishAot=false",
                "-p:DebugType=None",
                "-p:DebugSymbols=false",
                "-p:UseSharedCompilation=false",
                "-p:BuildInParallel=false",
                "-m:1",
                "--disable-build-servers",
                "-o",
                publish,
            ],
            output / f"publish-{mode}.log",
            project,
        )
        (publish / ".godot-dotnet-publish-mode").write_text(mode + "\n", encoding="utf-8")
        text = run(["dotnet", inspector / "InspectPayload.dll", publish, mode], output / f"payload-{mode}.json")
        reports[mode] = json.loads(text)

    def native(report):
        return {f["name"]: f["sha256"] for f in report["files"] if f["category"] == "native_runtime"}

    if native(reports["jit"]) != native(reports["trimmed-jit"]):
        raise RuntimeError("Native runtime differs between matched modes")
    expected_jar = hashlib.sha256(
        (ROOT / "modules/mono/thirdparty/libSystem.Security.Cryptography.Native.Android.jar").read_bytes()
    ).hexdigest()
    for report in reports.values():
        jar = next(f for f in report["files"] if f["name"] == "libSystem.Security.Cryptography.Native.Android.jar")
        if jar["sha256"] != expected_jar:
            raise RuntimeError("Published crypto JAR differs from precompiled APK template JAR")
    totals = {
        mode: {
            category: sum(f["bytes"] for f in report["files"] if f["category"] == category)
            for category in ("managed", "native_runtime", "excluded_static_link_input", "other")
        }
        for mode, report in reports.items()
    }
    if totals["trimmed-jit"]["managed"] >= totals["jit"]["managed"]:
        raise RuntimeError("Trimmed managed payload did not shrink")
    summary = {
        "source_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        "sdk": actual_sdk,
        "runtime": "10.0.12",
        "rid": "android-arm64",
        "godot_packages": version,
        "bytes": totals,
        "native_runtime_identical": True,
        "crypto_jar_matches_template": True,
        "android_device_runtime_test": False,
        "apk_export_test": False,
    }
    (output / "comparison.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
