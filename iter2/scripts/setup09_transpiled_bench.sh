#!/bin/bash
# setup09: Transpilat benchmarken (generiertes Release-Binary, 5 Runden).
# Usage: setup09_transpiled_bench.sh [path]
#   Ohne Argument wird das Fixture gebencht, mit Argument der reale Pfad.
set -euo pipefail
ITER2="$(cd "$(dirname "$0")/.." && pwd)"
FIXTURE="${FIXTURE:-/tmp/treemap_iter2_bench}"
TARGET="${1:-$FIXTURE}"
if [ ! -d "$TARGET" ]; then
  echo "error: bench path does not exist or is not a directory: $TARGET" >&2
  exit 1
fi
"$ITER2/cpp/build/gen-bench/treemap_cpp" --bench "$TARGET"
