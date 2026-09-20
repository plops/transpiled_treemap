# Walkthrough: Iteration 2 (Rust direkt + C++-Port + C++-Transpilat)

Datum: 2026-09-20 · Stand: implementiert, verifiziert, committet.

## Was wirklich implementiert wurde

1. **Umzug**: alter Stand per `git mv` nach `iter1/` eingefroren
   (eigener Commit, Historie erhalten). Alles Neue in `iter2/`.
2. **Rust (`iter2/`, Crate `treemap_iter2`)**: serieller Referenz-Scan +
   `squarify` plus parallele Variante (`std::thread::scope`,
   Owned-Subtrees, Cap 4×nCPU + `depth` von Anfang an, Re-Sort nach Join).
   `--scan`/`--bench` fensterlos, GUI auf parallelem Pfad.
   Tests: 4 Unit + 3 Headless (inkl. Broken-Pipe, Missing-Dir) — grün;
   `fmt`/`clippy -D warnings` sauber.
3. **C++ direkt (`iter2/cpp/src/`)**: `010`/`015`/`020` wie Rust
   (`error_code` + Mutex-`report_skip`, Thread-Fanout mit Cap,
   Top-Level-only-Layout), `090_main` (`--scan`/`--bench`), `030`/`095`
   (PGE3-Schale), `core_tests`. `ctest` (ASan/UBSan) grün, `--scan`
   byte-identisch zu Rust, Bench 6–7,5x (Fixture) / ~2,1x (groß).
   iter2-Fix: Pfade by value statt `const&` an Iterator-Interna.
4. **Transpilat (`iter2/lisp/gen.lisp` → `iter2/cpp/gen/`)**:
   Emitter-`defun`s ≤ 60 Zeilen, Paren-Disziplin (`parenmedic` grün vor
   jedem Lauf, Backups). Generat kompiliert (beide Presets), `ctest`
   grün, `--scan` byte-identisch zum direkten C++, Bench auf Parität
   (6,3–7,7x / ~1,9–2,7x). Erstmals wirklich als volle TU kompilierbar —
   iter1-Generat trug denselben Code mit ungeprüften Defekten.
5. **Skripte `iter2/scripts/setup01` … `setup09`**: Rust build/test/bench,
   C++ build/test-bench, Transpilat erstellen/compilieren/testen/
   benchmarken — alle ausführbar, alle im Lauf belegt.

## Testbedingte Änderungen (Befunde mit Fix)

- **Generat-Missing-Dir-Divergenz** (`in 1 files` statt `in 0 files`):
  `defstruct0` ignoriert Init-Forms → generierte `Node` ohne
  `isDir=true`. Fix: explizite Zuweisung im Emitter. Paritätstest fängt das.
- **Quoted-Includes vs. Gen-Build**: `src/030` zog immer `src/010`
  (eigene-Verzeichnis-Regel) → Redefinition im Gen-Build. Fix:
  Angle-Bracket-Includes mit `-I`-Reihenfolge (`gen` zuerst).
- **`fs`-Alias auf Global-Scope** im Generat (`treemap::fs` fehlte):
  Emitter-Hatch in den `treemap`-Namespace verschoben.
- **Emitter-Formen**: Vektor-`let`s als Call-Form (`()` nicht vergessen:
  offenes vs. leeres `let` beachten!), Forward-Deklarationen für
  `scan_tree`/`scan_tree_parallel_impl`/`squarify_parallel_depth`,
  `units`-Array als String-Hatch, `cast` statt `coerce`, Statement-`if`
  für Braced-Init, Double-Null via `(sum2-sum2)` (`0.0`→`0.F`-Bug),
  `thickness` als Float gegen `-Wnarrowing`.
- **Paren-Kaskaden**: zwei fehlende Schließer beim Abschreiben (Emacs-Regel
  aus dem Auftrag bestätigt); `parenmedic` + SBCL-Reader als Orakel
  (Top-Level-Formen einzeln lesen) finden die Stelle in Minuten.
- **GUI-Smokes**: Rust und C++ (unter `openbox`) Exit 124, keine Panics.

## Transpiler-Lücken (Upstream-Vorschläge cl-cpp-generator2)

1. `defstruct0` ignoriert dritte Slot-Angabe (Init-Form) kommentarlos —
   Vorschlag: Default-Member-Init emittieren oder warnen.
2. `0.0`-Literal → `0.F` (bestätigt, Workaround `(sum2-sum2)`).
3. `curly`-Strings → `initializer_list` ohne `operator[]` (Workaround:
   String-Hatch mit C-Array).
4. Vektor-`let` ohne Call-Form → `auto x = std::vector<T>;` (Workaround
   dokumentiert, aber leicht zu vergessen — Doku-Hinweis wünschenswert).
5. Wert-`if` mit Braces → ungültiges Ternär (Workaround Statement-`if`).

## Learnings

- Generierter Code ist erst nach Kompilat als volle TU + Byte-Parität +
  Kanten-Test (Missing-Dir!) vertrauenswürdig — Augenschein reicht nicht.
- Das `isDir`-Default zeigt die Safety-Antwort in klein: Kein Erbe, nur
  Disziplin + Tests + Sanitizer; der Paritätstest hat den echten Bug
  gefunden, kein Review.
- Cap/`depth`/Single-Stat von Tag 1 einbauen, nicht als Bench-Fix.
- Angle-Bracket-Includes für generierbare Header von Anfang an.

## Mögliche Erweiterungen

Cushion-Shading; `rayon`-Mess-Arm (kein Bedarf belegt); dir-Farb-Default
im Generat angleichen (kosmetisch); `gen.lisp` auf `090`/`030` ausweiten;
TSan-Preset; tiefe-Baum-Lasttest (Stack pro Thread).

## Neue Programme für den Docker-Container

Laufzeit: `xvfb libxkbcommon0 libxi6 libx11-6 libgl1`. Build:
`libx11-dev libxi-dev libgl1-mesa-dev libasound2-dev cmake g++ sbcl ninja-build`.
GUI-Smoke: `openbox x11-apps`. Lint: `clang-tidy clang-format`.
Paren-Tool: `parenmedic` (lokal). Fixture: `fallocate` (util-linux).
