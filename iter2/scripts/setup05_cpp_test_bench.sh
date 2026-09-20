#!/bin/bash
# setup05: direkten C++-Code testen (ctest, Sanitizer-Build) und benchen.
# Prüft zusätzlich --scan-Byte-Parität gegen den Rust-Referenz-Build.
# Usage: setup05_cpp_test_bench.sh [path]
#   Ohne Argument werden Fixture (FIXTURE bzw. Default) geprueft/gebencht,
#   mit Argument stattdessen dieser reale Pfad.
set -euo pipefail
ITER2="$(cd "$(dirname "$0")/.." && pwd)"
FIXTURE="${FIXTURE:-/tmp/treemap_iter2_bench}"
TARGET="${1:-$FIXTURE}"
if [ ! -d "$TARGET" ]; then
  echo "error: bench path does not exist or is not a directory: $TARGET" >&2
  exit 1
fi
ctest --test-dir "$ITER2/cpp/build/sanitized" --output-on-failure
diff <("$ITER2/target/release/treemap_iter2" --scan "$TARGET") \
     <("$ITER2/cpp/build/bench/treemap_cpp" --scan "$TARGET") \
  && echo "scan parity rust==cpp: OK"
"$ITER2/cpp/build/bench/treemap_cpp" --bench "$TARGET"
