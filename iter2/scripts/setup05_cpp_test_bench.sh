#!/bin/bash
# setup05: direkten C++-Code testen (ctest, Sanitizer-Build) und benchen.
# Prüft zusätzlich --scan-Byte-Parität gegen den Rust-Referenz-Build.
set -euo pipefail
ITER2="$(cd "$(dirname "$0")/.." && pwd)"
FIXTURE="${FIXTURE:-/tmp/treemap_iter2_bench}"
ctest --test-dir "$ITER2/cpp/build/sanitized" --output-on-failure
diff <("$ITER2/target/release/treemap_iter2" --scan "$FIXTURE") \
     <("$ITER2/cpp/build/bench/treemap_cpp" --scan "$FIXTURE") \
  && echo "scan parity rust==cpp: OK"
"$ITER2/cpp/build/bench/treemap_cpp" --bench "$FIXTURE"
