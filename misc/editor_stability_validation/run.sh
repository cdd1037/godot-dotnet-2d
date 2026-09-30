#!/usr/bin/env bash
# Focused headless-editor smoke; does not need a managed project or export templates.
set -euo pipefail
engine=$(realpath "${1:?Usage: run.sh ENGINE NEW_OUTPUT_DIR}")
output=$(realpath -m "${2:?Usage: run.sh ENGINE NEW_OUTPUT_DIR}")
if [[ -e "$output/project" ]]; then
    echo "Refusing to overwrite existing fixture: $output/project" >&2
    exit 2
fi
mkdir -p "$output/project"
cat > "$output/project/project.godot" <<'PROJECT'
config_version=5

[application]
config/name="Editor stability regression"
PROJECT
cat > "$output/project/missing_platform.gdextension" <<'EXTENSION'
[configuration]
entry_symbol = "test_library_init"
compatibility_minimum = "4.1"

[libraries]
windows.debug.x86_64 = "res://missing.dll"
windows.release.x86_64 = "res://missing.dll"
EXTENSION
sha256sum "$output/project/project.godot" > "$output/project-before.sha256"
timeout 45s "$engine" --headless --editor --path "$output/project" --import > "$output/import.log" 2>&1
sha256sum "$output/project/project.godot" > "$output/project-after.sha256"
cmp "$output/project-before.sha256" "$output/project-after.sha256"
grep -q 'No GDExtension library found' "$output/import.log"
if grep -Eq 'CRASH|Segmentation fault|Aborted' "$output/import.log"; then
    echo "Unexpected native failure in $output/import.log" >&2
    exit 1
fi
echo 'PASS: unsupported-platform extension reports an error and import exits; project.godot unchanged'
