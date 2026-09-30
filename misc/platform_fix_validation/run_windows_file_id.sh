#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
output=$(realpath -m "${1:?Usage: run_windows_file_id.sh OUTPUT_DIRECTORY}")
mkdir -p "$output"
python3 - "$root" "$output" <<'PY'
from pathlib import Path
import sys
root, output = map(Path, sys.argv[1:])
source = (root / 'drivers/windows/dir_access_windows.cpp').read_text()
start = source.index('typedef struct {\n\tULONGLONG LowPart;')
end = source.index('\nbool DirAccessWindows::is_link(', start)
(output / 'windows_file_id_impl.inc').write_text(source[start:end])
PY
"${CXX:-g++}" -std=c++17 -O2 -fsanitize=undefined -fno-sanitize-recover=undefined \
    -I"$output" "$root/misc/platform_fix_validation/windows_file_id.cpp" -o "$output/windows_file_id"
"$output/windows_file_id"
echo 'PASS: 13 mocked Win32 paths, ID comparisons and handle cleanup; no real Windows runtime tested'
