#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
output=$(realpath -m "${1:?Usage: run.sh OUTPUT_DIRECTORY}")
mkdir -p "$output"
"${CXX:-g++}" -std=c++17 -O2 -DUBSAN_ENABLED -fsanitize=undefined -fno-sanitize-recover=undefined \
    -I"$root" -I"$root/platform/linuxbsd" "$root/misc/integer_math_validation/overflow_ubsan.cpp" \
    -o "$output/overflow_ubsan"
"$output/overflow_ubsan"
echo 'PASS: native integer quotient overflow probe under UBSan'
