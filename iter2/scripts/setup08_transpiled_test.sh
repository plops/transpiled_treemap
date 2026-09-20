#!/bin/bash
# setup08: Transpilat testen (ctest + --scan-Parität gegen direktes C++).
set -euo pipefail
ITER2="$(cd "$(dirname "$0")/.." && pwd)"
FIXTURE="${FIXTURE:-/tmp/treemap_iter2_bench}"
ctest --test-dir "$ITER2/cpp/build/gen-sanitized" --output-on-failure
diff <("$ITER2/cpp/build/bench/treemap_cpp" --scan "$FIXTURE") \
     <("$ITER2/cpp/build/gen-bench/treemap_cpp" --scan "$FIXTURE") \
  && echo "scan parity direct==generated: OK"
"$ITER2/cpp/build/gen-bench/treemap_cpp" --scan /nonexistent-treemap-dir-xyz
