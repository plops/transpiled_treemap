#!/bin/bash
# setup07: Transpilat compilieren (Syntax-Check + CMake-Builds wie setup04).
set -euo pipefail
ITER2="$(cd "$(dirname "$0")/.." && pwd)"
for h in 010_scanner 015_color 020_layout; do
  g++ -std=c++20 -fsyntax-only -I"$ITER2/cpp/gen" -x c++ - <<<"#include \"$h.hpp\"
int main() { return 0; }"
done
cmake -S "$ITER2/cpp" -B "$ITER2/cpp/build/gen-sanitized" -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON -DENABLE_UBSAN=ON -DUSE_GENERATED=ON
cmake --build "$ITER2/cpp/build/gen-sanitized"
cmake -S "$ITER2/cpp" -B "$ITER2/cpp/build/gen-bench" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DENABLE_ASAN=OFF -DENABLE_UBSAN=OFF -DUSE_GENERATED=ON
cmake --build "$ITER2/cpp/build/gen-bench"
