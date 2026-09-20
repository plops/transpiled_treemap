# task.md — Implementierungs- und Testschritte (20260920_02_redo, iter2)

Jeder Schritt endet mit Validierung; erst bei Grün weiter.
Schritte seriell abarbeiten. Skripte: `iter2/scripts/setupNN_*.sh`.
Paren-Regel: vor jeder Lisp-Edit-Sitzung
`cp lisp/gen.lisp lisp/gen.lisp.known-good`; vor jedem Lisp-Commit
`parenmedic diagnose` + `sbcl`-Lauf grün.

## Schritt 0 — Umzug & Gerüst

1. `mkdir -p iter1 iter2 && git mv Cargo.toml Cargo.lock src tests cpp
   iter1/` (+ `cpp`-nahe Dotfiles soweit vorhanden); Root-`.gitignore`
   und `plan/` bleiben. `git status --short` prüfen.
2. `iter2/`-Gerüst: `Cargo.toml` (neuer Paketname, `macroquad`-Version
   nach Newest-Check), `src/main.rs`-Stub, `tests/`-Stub, `cpp/`
   (CMake-Skelett mit Sanitizer- und Bench-Preset), `lisp/gen.lisp`-Stub,
   `scripts/setup01_*.sh` … `setup09_*.sh` (Stubs mit `set -euo pipefail`),
   Fixture-Generator-Skript (deterministisch, 4000-Dateien-Baum).
3. `cargo search macroquad` (+ Kandidaten bei Bedarf) protokollieren;
   `git log --oneline -- iter1 | head` zeigt Historie.
4. Commit: `chore(iter2-scripts): umzug nach iter1 und iter2-geruest`.

Gate: `iter1/` baut/testet wie zuvor; `setup01` … `setup09` existieren
und sind ausführbar (`ls -l iter2/scripts/setup*.sh`).

## Schritt 1 — Rust seriell (direkt)

1. `Node`, `report_skip`, `scan_tree`/`scan_entry` (rekursiv,
   Symlink-Skip, `/proc|/sys|/dev`-Filter, `size > 0`-Filter),
   `worst`/`layout_row`/`squarify`, `render_tree`, `color_for_path`,
   `format_bytes`, `print_line` (BrokenPipe-sicher), `--scan`/`--bench`-CLI.
2. `setup01_rust_build.sh` finalisieren (Debug + Release).
3. Commit: `feat(iter2-rust): serieller referenz-stand mit scan-bench`.

Gate: `setup01_rust_build.sh`; `cargo fmt --check`;
`cargo clippy --all-targets -- -D warnings`.

## Schritt 2 — Rust parallel + Tests & Bench

1. `scan_tree_parallel` (`std::thread::scope`, nur Subdirs, Cap 4×nCPU,
   `depth`-Parameter), `squarify_parallel` (Top-Level-only),
   Re-Sort nach Join.
2. Unit-Tests: `format_bytes`, seriell/parallel-Scan-Äquivalenz,
   Layout-Invarianten (Fläche < 0,5 %, Sortierung, keine Überlappung).
3. `setup02_rust_test.sh`, `setup03_rust_bench.sh` finalisieren
   (Median ≥ 5 Läufe, Fix-Fixture).
4. Commit: `test(iter2-rust): seriell-vs-parallel aequivalenz und bench`.

Gate: `setup02_rust_test.sh` grün; `setup03_rust_bench.sh` druckt
Scan-/Layout-Zeiten + Speedup (Tabelle notieren).

## Schritt 3 — Rust-Smoke

1. Integration (`tests/headless_scan.rs` o.ä.): `--scan`-Ausgabe,
   Broken-Pipe (`| head -n 1`, Exit 0), fehlendes Verzeichnis (skip,
   kein Panic), Filter-Kanten (Symlink, 0-Byte, `/proc`).
2. `xvfb-run -a timeout 20 <bin> <fixture>` seriell + parallel.
3. Commit: `test(iter2-rust): headless und xvfb-smoke`.

Gate: Exit 124, stderr ohne Panic; `cargo test` grün.

## Schritt 4 — Direkter C++-Port

1. `010_scanner.hpp` (Typen, `report_skip` mit Mutex, seriell + parallel
   mit Fan-out-Cap), `015_color.hpp` (Palette + `format_bytes`),
   `020_layout.hpp` (seriell + parallel wie Rust), `090_main.cpp`
   (`--scan`/`--bench`), `030_pge_app.hpp` + GUI-Main (PGE3-Schale).
2. `setup04_cpp_build.sh` (Sanitizer- + Bench-Preset),
   `setup05_cpp_test_bench.sh` (`ctest`, `--scan`-Diff, `--bench`)
   finalisieren. Single-Stat-Scan (`is_symlink/is_directory/
   is_regular_file`) von Anfang an.
3. GUI-Smoke nur unter `openbox` (`xvfb` allein → BadAtom).
4. Commits: `feat(iter2-cpp): direkter port von scan und layout`,
   `test(iter2-cpp): byte-paritaet und ctest`.

Gate: `setup04_cpp_build.sh`; `setup05_cpp_test_bench.sh` grün;
`diff <(rust --scan) <(cpp --scan)` leer; Bench-Tabelle vorhanden.

## Schritt 5 — Transpiler-Input (C++-Core)

1. `lisp/gen.lisp` in Emitter-`defun`s ≤ 60 Zeilen schreiben
   (Backup-Disziplin, Closer auf eigenen Zeilen, Hatches nur mit
   Präzedenz-Beleg aus `SUPPORTED_FORMS.md`).
2. `setup06_transpile.sh` (`sbcl`-Lauf + `parenmedic diagnose`),
   `setup07_transpiled_build.sh` (`-fsyntax-only` + CMake),
   `setup08_transpiled_test.sh` (`ctest` + `--scan`-Diff),
   `setup09_transpiled_bench.sh` finalisieren.
3. Transpiler-Lücken mit Minimalbeispiel notieren (Walkthrough-Kandidaten).
4. Commits: `feat(iter2-lisp): <emitter-modul> mit known-good-backup`
   (pro Modul ein Commit).

Gate: `setup06` … `setup09` grün; Generat-`--scan`-Diff leer;
`g++ -fsyntax-only` grün.

## Schritt 6 — Dokumente & Abschluss

1. `deps.md` final (Newest-Stände, Usage-Examples, Orgs, Docker-Pakete).
2. `report_rust_cpp.md`: Mess-Tabellen (kleiner + großer Baum),
   Variablen-Paritäts-Tabelle mit Befund je Zeile, Safety-Fazit
   (kein Erbe, nur Disziplin + Tests + Sanitizer).
3. `walkthrough.md`: was wirklich implementiert, testbedingte Änderungen
   mit Messwerten, Transpiler-Lücken mit Beispiel, Learnings,
   Erweiterungen, neue Docker-Programme.
4. Commit: `docs(plan): walkthrough und rust-cpp-report`.

Gate: alle Skripte 01–09 belegt; `git status --short` nur beabsichtigte
Pfade; alle Tests/Smokes/Benches in dieser Session grün gesehen.
