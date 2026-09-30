#!/usr/bin/env bash
# Run standalone tests against the actual vendored glslang implementation.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../.." && pwd)
output=${1:-$(mktemp -d)}
mkdir -p "$output"
output=$(cd "$output" && pwd)
cd "$repo"
compiler=${CXX:-c++}
"$compiler" -std=c++17 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -Ithirdparty/glslang "$here/token_bounds.cpp" -o "$output/token_bounds"
"$output/token_bounds" | tee "$output/token-bounds.log"
if [[ -z ${GLSLANG_ARCHIVE:-} ]]; then
    echo 'SKIP runtime tests: set GLSLANG_ARCHIVE to an existing Godot libmodule_glslang archive.'
    exit 0
fi
if [[ ! -f "$GLSLANG_ARCHIVE" ]]; then
    echo "Missing archive: $GLSLANG_ARCHIVE" >&2
    exit 1
fi
mkdir -p "$output/obj"
# Rebuild every translation unit including the changed production header.
for f in $(grep -R -l 'PpContext.h' thirdparty/glslang --include='*.cpp'); do
    echo "Compiling $f"
    "$compiler" -std=c++17 -O0 -g0 -DENABLE_OPT=0 -Ithirdparty/glslang -Ithirdparty -Ithirdparty/spirv-headers/include/spirv/unified1 -c "$f" -o "$output/obj/$(basename "$f" .cpp).o"
done
# Reuse unchanged objects. Explicit rebuilt objects supersede archive members.
"$compiler" -std=c++17 -O0 -Ithirdparty/glslang "$here/preprocessor_runtime.cpp" "$output"/obj/*.o "$GLSLANG_ARCHIVE" -pthread -o "$output/preprocessor_runtime"
"$output/preprocessor_runtime" | tee "$output/preprocessor-runtime.log"
