#!/bin/bash
# setup03: Rust-Code benchmarken (seriell vs parallel, 5 Runden).
# Erstellt deterministisch das 4000-Dateien-Fixture (40x100x1 KiB).
set -euo pipefail
ITER2="$(cd "$(dirname "$0")/.." && pwd)"
FIXTURE="${FIXTURE:-/tmp/treemap_iter2_bench}"
if [ ! -f "$FIXTURE/.ready" ]; then
  rm -rf "$FIXTURE"
  mkdir -p "$FIXTURE"
  for d in $(seq -w 1 40); do
    mkdir -p "$FIXTURE/d$d"
    for f in $(seq -w 1 100); do
      fallocate -l 1K "$FIXTURE/d$d/f$f.bin"
    done
  done
  touch "$FIXTURE/.ready"
fi
cd "$ITER2"
cargo build --release
./target/release/treemap_iter2 --bench "$FIXTURE"
