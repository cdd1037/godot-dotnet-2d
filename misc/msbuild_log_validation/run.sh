#!/usr/bin/env bash
set -euo pipefail
# Requires dotnet on PATH and matching built GodotSharp API/editor assemblies.
root=$(cd "$(dirname "$0")/../.." && pwd)
editor=${1:?Usage: run.sh /absolute/path/to/mono/editor /absolute/path/to/output}
output=${2:?Specify an isolated output directory}
editor=$(realpath "$editor")
mkdir -p "$output"
output=$(realpath "$output")
project="$output/runtime"
mkdir -p "$project/addons/msbuild_tests"
cp "$root/misc/msbuild_log_validation/"{MSBuildLogTests.csproj,project.godot} "$project/"
cp "$root/misc/msbuild_log_validation/addons/msbuild_tests/"{TestPlugin.cs,plugin.cfg} "$project/addons/msbuild_tests/"
cat >"$project/NuGet.Config" <<XML
<configuration><packageSources><clear/><add key="fork" value="$root/bin/GodotSharp/Tools/nupkgs"/></packageSources></configuration>
XML
dotnet build "$root/modules/mono/editor/GodotTools/GodotTools.sln" -c Debug -m:1 >"$output/build-tools.log" 2>&1
dotnet build "$project/MSBuildLogTests.csproj" >"$output/build-tests.log" 2>&1
timeout 120 "$editor" --headless --editor --path "$project" >"$output/runtime-test.log" 2>&1
cat "$output/runtime-test.log"
grep -q '^MSBUILD_LOG_TESTS_PASSED$' "$output/runtime-test.log"
