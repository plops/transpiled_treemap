#!/bin/bash
# setup06: C++-Transpilat erstellen (sbcl-Lauf + parenmedic-Gate).
set -euo pipefail
ITER2="$(cd "$(dirname "$0")/.." && pwd)"
cp "$ITER2/lisp/gen.lisp" "$ITER2/lisp/gen.lisp.known-good"
/workspace/src/parenmedic/zig-out/bin/parenmedic diagnose "$ITER2/lisp/gen.lisp"
sbcl --load "$ITER2/lisp/gen.lisp" --quit
cp "$ITER2"/lisp/gen/*.hpp "$ITER2/cpp/gen/"
