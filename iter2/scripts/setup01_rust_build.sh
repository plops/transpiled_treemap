#!/bin/bash
# setup01: Rust-Code compilieren (Debug + Release).
set -euo pipefail
ITER2="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ITER2"
cargo build
cargo build --release
