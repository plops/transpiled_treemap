#!/bin/bash
# setup04: direkten C++-Code compilieren.
# Sanitizer-Build (Tests) + sanitize-freier Release-Build (Bench).
set -euo pipefail
ITER2="$(cd "$(dirname "$0")/.." && pwd)"
cmake -S "$ITER2/cpp" -B "$ITER2/cpp/build/sanitized" -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON -DENABLE_UBSAN=ON
cmake --build "$ITER2/cpp/build/sanitized"
cmake -S "$ITER2/cpp" -B "$ITER2/cpp/build/bench" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DENABLE_ASAN=OFF -DENABLE_UBSAN=OFF
cmake --build "$ITER2/cpp/build/bench"
