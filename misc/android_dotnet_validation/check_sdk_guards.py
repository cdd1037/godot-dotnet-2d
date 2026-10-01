#!/usr/bin/env python3
"""Exercise real SDK evaluation and validation without NuGet restore or ILLink."""

import argparse
import json
import shutil
import subprocess
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="New empty evidence directory")
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists() and any(output.iterdir()):
        parser.error("Output must be empty")
    output.mkdir(parents=True, exist_ok=True)
    sdk = output / "Sdk"
    shutil.copytree(ROOT / "modules/mono/editor/Godot.NET.Sdk/Godot.NET.Sdk/Sdk", sdk)
    # References are disabled: only the production SDK's property/guard targets run.
    (sdk / "SdkPackageVersions.props").write_text("<Project />", encoding="utf-8")
    cases = [
        ("mono-jit", {}, [], None),
        ("mono-trim-project-property", {"PublishTrimmed": "true"}, [], None),
        ("mono-trim-cli-property", {}, ["PublishTrimmed=true"], None),
        ("android-aot", {}, ["PublishAot=true"], "Android exports require .NET 10 Mono JIT"),
        ("android-coreclr", {}, ["UseMonoRuntime=false"], "Android exports require .NET 10 Mono JIT"),
        ("wrong-runtime", {}, ["RuntimeFrameworkVersion=10.0.11"], "Android runtime must be 10.0.12"),
        ("android-bionic", {}, ["RuntimeIdentifier=linux-bionic-arm64"], "Android CoreCLR runtime identifiers"),
        ("trim-editor", {"PublishTrimmed": "true"}, ["Configuration=Debug"], "Godot editor assemblies require JIT"),
        (
            "trim-legacy",
            {"PublishTrimmed": "true"},
            ["GodotSupportLegacyNonTrimSafeAPIs=true"],
            "require rebuilt script metadata",
        ),
        (
            "desktop-trim-project-property",
            {"PublishTrimmed": "true"},
            ["RuntimeIdentifier=linux-x64", "GodotTargetPlatform=linuxbsd"],
            None,
        ),
    ]
    results = []
    for name, properties, overrides, error in cases:
        project = ET.Element("Project")
        ET.SubElement(project, "Import", Project=str(sdk / "Sdk.props"))
        group = ET.SubElement(project, "PropertyGroup")
        for key, value in {
            "TargetFramework": "net10.0",
            "DisableImplicitGodotGeneratorReferences": "true",
            "DisableImplicitGodotSharpReferences": "true",
            **properties,
        }.items():
            ET.SubElement(group, key).text = value
        ET.SubElement(project, "Import", Project=str(sdk / "Sdk.targets"))
        path = output / f"{name}.csproj"
        ET.ElementTree(project).write(path, encoding="utf-8", xml_declaration=True)
        command = [
            "dotnet",
            "msbuild",
            str(path),
            "-nologo",
            "-t:ValidateGodotPlatform;ValidateGodotTrimMode",
            "-p:RuntimeIdentifier=android-arm64",
            "-p:Configuration=ExportRelease",
        ]
        command += [f"-p:{value}" for value in overrides]
        command += [
            "-getProperty:DefineConstants,UseMonoRuntime,RuntimeFrameworkVersion,_EnableConsumingManagedCodeFromNativeHosting"
        ]
        result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (output / f"{name}.log").write_text(result.stdout, encoding="utf-8")
        if error:
            assert result.returncode != 0 and error in result.stdout, (name, result.stdout)
        else:
            assert result.returncode == 0, (name, result.stdout)
            data = json.loads(result.stdout[result.stdout.index("{") :])["Properties"]
            trim = "trim" in name
            assert ("GODOT_TRIMMED" in data["DefineConstants"].split(";")) == trim, (name, data)
            desktop = name.startswith("desktop")
            assert (data["_EnableConsumingManagedCodeFromNativeHosting"] == "true") == (trim and desktop), (name, data)
            if not desktop:
                assert data["UseMonoRuntime"] == "true" and data["RuntimeFrameworkVersion"] == "10.0.12", (name, data)
        results.append({"case": name, "passed": True, "expected_rejection": error is not None})
    (output / "results.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
    print(f"{len(results)} SDK evaluation/guard cases passed")


if __name__ == "__main__":
    main()
