#!/bin/bash
# setup02: Rust-Code testen (Unit + Integration) plus fmt/clippy-Gates.
set -euo pipefail
ITER2="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ITER2"
cargo test
cargo fmt --check
cargo clippy --all-targets -- -D warnings
