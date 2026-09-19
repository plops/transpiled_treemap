# task.md — Implementierungs- und Testschritte (20260919_01_merge)

Jeder Schritt endet mit Validierung; erst bei Grün weiter. Richtungsentscheidung
(19.09., wol pumba): Rust + C++ werden **direkt** geschrieben (kein Rust-Transpiler),
`cpp/gen.lisp` (cl-cpp-generator2) erst am Ende für den fertigen C++-Code.

## Schritt 0 — Baseline & Toolchain ✅

1. Referenz `00_mvp` gelesen (286 Zeilen); `pge_treemap`-Schale gelesen.
2. `cargo search rayon` → 1.12.0, `macroquad` → 0.4.16 (= Lock-Stand).
3. Container: `apt-get install xvfb libx11-dev libxi-dev libgl1-mesa-dev
   libasound2-dev libxkbcommon0` (X11-Build + xvfb-Smoke).
4. Transpiler-Spikes nach `/tmp` (write-source-Signatur, scope/move-Closure).

## Schritt 1 — Rust direkt ✅

1. `Cargo.toml` (hand, `macroquad 0.4`), `src/main.rs`: Node, report_skip,
   scan_tree/scan_entry (seriell), scan_tree_parallel ( scope, nur Subdirs),
   worst/layout_row/squarify/squarify_level/layout_subtree_parallel/
   squarify_parallel, render/color/format_bytes/print_line,
   --scan/--bench (plain main, kein Fenster), GUI via
   `macroquad::Window::from_config` (Muster 01_more).
2. ✅ `cargo build`, `cargo fmt --check`,
   `cargo clippy --all-targets -- -D warnings`.

## Schritt 2 — Rust-Tests & Bench ✅

1. Unit (`src/main.rs`): format_bytes, seriell/parallel-Scan-Äquivalenz,
   Layout-Invarianten (Fläche < 0,5 %, Sortierung), Scan-Bench-Print.
2. Integration (`tests/headless_scan.rs`): --scan-Ausgabe, Broken-Pipe
   (`| head -n 1`, Exit 0), fehlendes Verzeichnis (skip, kein Panic).
3. ✅ `cargo test`: 4 + 3 grün.
4. Fixture 4000 Dateien (`/tmp/bench_tree`, 40×100×1 KiB):
   `./target/release/treemap_parallel --bench` → Scan-Speedup ~4–5x.
   Zwischenbefund dokumentiert: Thread-pro-Datei war 12x langsamer
   (→ nur Subdirs), fehlendes Re-Sort nach Join brach Sortier-Invariante
   (→ `sort_unstable_by_key` nach Join).

## Schritt 3 — GUI-Smoke ✅

- `xvfb-run -a timeout 20 ./target/debug/treemap_parallel /tmp/bench_tree`
  → Exit 124, stderr zeigt target + layout (size=4096000), kein Panic.

## Schritt 4 — C++-Port ✅

1. `cpp/src/010_scanner.hpp` (Node/Rect/Color, report_skip mit Mutex,
   scan_tree/scan_entry, scan_tree_parallel mit std::thread-Fanout),
   `015_color.hpp` (Palette + Hash-Fallback + format_bytes),
   `020_layout.hpp` (serial + parallel wie Rust),
   `090_main.cpp` (--scan/--bench), `030_pge_app.hpp` (PGE3-Schale,
   `#ifdef TREEMAP_HAS_PGE3`, ohne Header nicht kompiliert),
   `cpp/tests/core_tests.cpp`, `cpp/CMakeLists.txt` (ASan/UBSan default ON).
2. ✅ `cmake --build`, `ctest` grün, `--scan` byte-identisch zu Rust,
   `--bench` Scan-Speedup ~4,2–4,6x.

## Schritt 5 — Transpiler-Input C++ ✅

1. `cpp/gen.lisp` (cl-cpp-generator2, Muster 09_f entertainment + freestanding
   example): Emitter-`defun`s ≤ 60 Zeilen, `lprint`-Helper,
   String-Hatches nur mit Präzedenz (lock_guard-Muster aus 09).
2. ✅ `sbcl --load cpp/gen.lisp --quit` schreibt `cpp/gen/*.hpp`;
   `parenmedic diagnose` grün; `g++ -fsyntax-only` grün;
   Binary aus generierten Headern: --scan identisch, --bench grün,
   Missing-Dir sicher.
3. Emitter-Fixes unterwegs (Walkthrough): `foreach` statt `for`,
   `logior/logand` statt `or/and`, `incf` statt `+=`, `(Node)`-Ctor,
   `cast`/Funktional-Cast statt `coerce`, `?`-Ternär, `std--sort`,
   `curly`-Arrays, f-Suffix-Literale, keine `declare`-Formen,
   `.string()` per Hatch.

## Schritt 6 — Dokumente (dieser Schritt)

1. `task.md` (diese Datei), `deps.md`, `walkthrough.md` finalisieren.
2. Commits in Conventional Commits (s. plan.md), nur eigene Pfade,
   nie `target/`, `cpp/build/`, `/tmp`-Artefakte.
