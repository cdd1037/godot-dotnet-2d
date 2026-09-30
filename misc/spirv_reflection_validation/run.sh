#!/usr/bin/env bash
# Real glslang output and actual vendored reflection parser, with no GPU submission.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
output=$(realpath -m "${1:?Usage: run.sh OUTPUT_DIRECTORY [BASELINE_REF]}")
baseline=${2:-a743a9df}
archive=${GLSLANG_ARCHIVE:-$root/bin/obj/modules/libmodule_glslang.linuxbsd.editor.x86_64.a}
mkdir -p "$output/corpus"
cd "$root"
[[ -f "$archive" ]] || { echo 'Build the Mono editor/glslang archive first or set GLSLANG_ARCHIVE' >&2; exit 2; }
git show "$baseline:thirdparty/spirv-reflect/spirv_reflect.c" > "$output/baseline.c"
"${CXX:-c++}" -std=c++17 -O2 -Ithirdparty/glslang misc/spirv_reflection_validation/generate_corpus.cpp "$archive" -pthread -o "$output/generate_corpus"
"$output/generate_corpus" "$output/corpus" > "$output/corpus-generation.log"
for side in baseline current; do
    source_file="$output/baseline.c"
    [[ "$side" == baseline ]] || source_file=thirdparty/spirv-reflect/spirv_reflect.c
    "${CC:-gcc}" -std=c11 -O2 -Ithirdparty/spirv-reflect -c "$source_file" -o "$output/$side.o"
    "${CXX:-c++}" -std=c++17 -O2 -I. misc/spirv_reflection_validation/reflect_corpus.cpp "$output/$side.o" -o "$output/$side"
    "$output/$side" "$output"/corpus/*.spv > "$output/$side-reflection.txt" 2> "$output/$side-timing.log"
done
diff -u "$output/baseline-reflection.txt" "$output/current-reflection.txt" > "$output/reflection.diff"
"${CC:-gcc}" -std=c11 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -I. misc/spirv_reflection_validation/lookup_cases.c -o "$output/lookup_cases"
# LeakSanitizer cannot initialize under this executor's ptrace environment.
ASAN_OPTIONS=detect_leaks=0 "$output/lookup_cases" > "$output/lookup-tests.log"
echo 'PASS: four generated SPIR-V modules have equal reflection; lookup edge cases pass ASan/UBSan'
