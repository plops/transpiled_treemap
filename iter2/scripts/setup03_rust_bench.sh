#!/bin/bash
# setup03: Rust-Code benchmarken (seriell vs parallel, 5 Runden).
# Erstellt deterministisch das 4000-Dateien-Fixture (40x100x1 KiB).
# Usage: setup03_rust_bench.sh [path]
#   Ohne Argument wird das Fixture benutzt (erstellt falls noetig).
#   Mit Argument wird stattdessen dieser reale Pfad gebencht.
set -euo pipefail
ITER2="$(cd "$(dirname "$0")/.." && pwd)"
FIXTURE="${FIXTURE:-/tmp/treemap_iter2_bench}"
TARGET="${1:-$FIXTURE}"
if [ "$TARGET" = "$FIXTURE" ]; then
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
else
  if [ ! -d "$TARGET" ]; then
    echo "error: bench path does not exist or is not a directory: $TARGET" >&2
    exit 1
  fi
fi
cd "$ITER2"
cargo build --release
./target/release/treemap_iter2 --bench "$TARGET"
