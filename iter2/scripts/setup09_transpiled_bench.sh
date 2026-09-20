#!/bin/bash
# setup09: Transpilat benchmarken (generiertes Release-Binary, 5 Runden).
set -euo pipefail
ITER2="$(cd "$(dirname "$0")/.." && pwd)"
FIXTURE="${FIXTURE:-/tmp/treemap_iter2_bench}"
"$ITER2/cpp/build/gen-bench/treemap_cpp" --bench "$FIXTURE"
