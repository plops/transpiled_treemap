## Goal

Zweite Iteration des parallelen Treemaps in neuem Ordner `iter2/`: zuerst
direkt geschriebener Rust-Code (seriell als Referenz + parallel), getestet
und gebenchmarkt; danach semantisch äquivalenter, direkt geschriebener
C++-Code (ebenfalls seriell + parallel, getestet, gebenchmarkt); danach eine
transpilierte Reimplementierung des fertigen C++-Cores aus
`cl-cpp-generator2`-Lisp (`gen.lisp` → generierte Header), ebenfalls
compiliert, getestet und gebenchmarkt. Der existierende Stand (Rust
`src/` + `tests/` + `Cargo.*`, C++ `cpp/`) wird per `git mv` unverändert
nach `iter1/` verschoben und eingefroren. Alles Neue lebt in `iter2/`.
Parallele Pfade werden immer gegen die serielle Referenz verglichen.
Am Ende: `walkthrough.md` + Vergleichsreport Rust vs. C++ in
`plan/20260920_02_redo/`.

## Success Criteria

- `iter1/` enthält den alten Stand per `git mv` (Historie erhalten),
  baut und testet wie zuvor; danach keine Änderungen mehr dort.
- `iter2/` enthält eigenen Rust-Crate (`Cargo.toml`, `src/main.rs`,
  `tests/`), eigenes C++-Projekt (`cpp/` mit CMake, Core-Header,
  `--scan`/`--bench`-Binaries, GUI-Schale), eigene `lisp/gen.lisp`
  mit generierten Headern sowie `scripts/setup01_*.sh` … `setup09_*.sh`.
- Rust: `cargo build`, `cargo fmt --check`,
  `cargo clippy --all-targets -- -D warnings` grün (serielles + paralleles Ziel).
- C++ direkt: CMake-Build grün, `ctest` grün (ASan/UBSan-Preset),
  `--scan` byte-identisch zu Rust `--scan`, `--bench` mit Speedup-Tabelle.
- Transpilat: `sbcl --load lisp/gen.lisp --quit` (Exit 0) schreibt generierte
  Header; `g++ -fsyntax-only` grün; generiertes Binary: `--scan`
  byte-identisch, Tests grün, `--bench` berichtet.
- Äquivalenz seriell vs. parallel automatisiert und grün (Größen-Summen,
  Flächen-Erhalt < 0,5 %, keine Geschwister-Überlappung, Sortier-Invariante),
  in Rust UND C++ UND Transpilat, auf Fixture-Baum + Real-Verzeichnis.
- Alle 9 Setup-Skripte existieren, sind ausführbar und im Walkthrough belegt.
- `task.md`-Schritte seriell mit Gates abgearbeitet, Zwischen-Commits in
  Conventional Commits, `walkthrough.md` + `report_rust_cpp.md` abgelegt.

## Context And Current Facts

- Eingefrorener Vorläufer: `iter1/` ≙ heutiger Root-Stand (`src/main.rs`
  672 Zeilen: `Node{path,size,is_dir,children,rect,color}`, `scan_tree` /
  `scan_entry` seriell, `scan_tree_parallel` via `std::thread::scope` nur
  über Subdirs, `worst`/`layout_row`/`squarify` + `squarify_parallel`
  Top-Level-only, `report_skip`-Helper, `print_line` BrokenPipe-sicher,
  `--scan`/`--bench`/GUI via `Window::from_config`; `tests/headless_scan.rs`;
  `cpp/src/010_scanner.hpp`, `015_color.hpp`, `020_layout.hpp`,
  `030_pge_app.hpp`, `090_main.cpp`, `095_gui_main.cpp`;
  `cpp/gen.lisp` 400 Zeilen + `cpp/gen.lisp.known-good`;
  `cpp/CMakeLists.txt` mit ASan/UBSan default ON). Details und Messwerte:
  `plan/20260919_01_merge/{rust_cpp_vergleich,walkthrough}.md`.
- Referenz-MVP (fremd, nur lesen): `/workspace/src/rs_disk_treemap/00_mvp/`
  (286 Zeilen, `macroquad`, kein Text im Treemap). GUI-Schalen-Kontrast:
  `/workspace/src/pge_treemap/source0/src/`.
- Transpiler-Sprache: `/workspace/src/cl-cpp-generator2/SUPPORTED_FORMS.md`
  (autogeneriert aus grünen Tests — einzige verbindliche Formen-Referenz).
  Strukturvorbild: `cl-cpp-generator2/example/197_shadertoy/gen4.lisp`;
  C++-Minimalbeispiel: `plan/20260919_01_merge/gen-cpp-freestanding-example.lisp`.
- Werkzeuge verifiziert: `rustc/cargo 1.98.1`, `rustup` (stable aktiv),
  `sbcl 2.6.0.debian`, `cmake 4.2.3`, `g++ 15.2.0`,
  `parenmedic` in `/workspace/src/parenmedic/zig-out/bin/parenmedic`
  (`diagnose`/`fix`), `xvfb-run`/`Xvfb`/`openbox`/`xdotool`/`xwd`,
  `clang-tidy`, `clang-format` vorhanden. deepwiki-MCP (`plops/...`) war im
  Container bisher ohne Transport — Ersatz: lokale Docs + `cargo info`.
- xvfb-Muster: `plan/20260918_02_review_and_xvfb_test/walkthrough.md` in
  `/workspace/src/rs_disk_treemap/plan/` (Exit-124-Smoke, EPIPE-Lehre,
  Docker-Pakete). PGE3 unter nacktem xvfb stirbt vor App-Code
  (`_NET_WM_STATE`-BadAtom) — Smoke braucht `openbox`.
- Bekannte Fallen aus Iter1 (einplanen, nicht wiederfinden):
  Thread-pro-Datei 12x langsamer → Threads nur pro Subdir; Join zerstört
  Sortierung → `sort` nach Join; 3 Stats/Datei in C++ (5,6x) →
  `is_symlink/is_directory/is_regular_file` (d_type-Cache, 1 Stat);
  unbegrenzter Fan-out 5x langsamer → Cap 4×nCPU (Scan) + Top-Level-only
  (Layout); `root.rect` im GUI-Worker nie gesetzt → Hover tot.

## Constraints And Non-goals

- Klein halten: keine neue Cargo-/CMake-Dep ohne Mess- oder
  Vereinfachungsbeleg; kein `criterion`/`tempfile`/`assert_cmd`
  (Bench via `std::time::Instant`, Black-Box via `std::process::Command`).
- `iter1/` und fremde Repos (`00_mvp`, `pge_treemap`, Generatoren) bleiben
  unverändert; `iter2` startet leer (kein Copy-Paste-Blindflug — bewusst
  neu schreiben, 1:1-Namensparität Rust↔C++ wie in Iter1).
- MVP-Charakter: kein Text im Treemap, keine Fonts, keine
  Kamera/Animation/Watcher; Cushion-Shading bleibt Erweiterung.
- Paren-Disziplin ist Gate: vor jedem Lisp-Commit `parenmedic diagnose`
  grün + `sbcl`-Generierungslauf grün; vor jeder Lisp-Edit-Sitzung
  `gen.lisp.known-good`-Backup; Emitter-`defun`s ≤ 60 Zeilen,
  Top-Level-Closer auf eigenen Zeilen (Emacs-Prüfbarkeit).
- Non-goals: `notify`-Upgrade, CJK-Breiten, `HashMap`-Merge ohne Beleg,
  Release-Workflow — nur als Walkthrough-Erweiterungen notieren.

## Key Decisions

1. **Layout: `git mv` nach `iter1/`, Neustart in `iter2/`.**
   `iter1/` = `Cargo.toml`, `Cargo.lock`, `src/`, `tests/`, `cpp/`
   (inkl. `.clang-format`/`.clang-tidy`, `third_party/`).
   Root-`.gitignore`, Root-`Cargo.toml`-Rolle und `plan/` bleiben am Root.
   `iter2/` = eigener Crate (`Cargo.toml`, `src/main.rs`, `tests/`),
   `cpp/` (CMake-Projekt), `lisp/gen.lisp` (+ `.known-good`),
   `scripts/setup01_*.sh` … `setup09_*.sh`, `fixtures/`-Doku.
   Alternative „iter2 als Cargo-Workspace-Member am Root" verworfen:
   eigener Ordner hält iter1 reproduzierbar und Skript-Pfade stabil.
2. **Rust zuerst, direkt geschrieben, `std`-only.**
   `std::thread::scope` + Owned-Subtrees, `depth`-Parameter und Fan-out-Cap
   von Anfang an (nicht als Bench-Fix hinterher). `rayon` nur als
   Mess-Arm bei Bedarf, Übernahme nur mit Speedup-Beleg.
3. **C++-Port semantisch äquivalent, Verantwortungs-Abbildung explizit:**
   `Result` → `std::error_code`-Out-Parameter + zentrale `report_skip`
   (stderr unter Mutex); Ownership → `std::move` über Thread-Grenzen +
   disjunkte `results[i]`-Slots; Scope-Join → `vector<thread>` + Join-Schleife
   im selben Scope + atomares Budget; `match` → `switch` mit
   `default`-Assert. snake_case bleibt für 1:1-Lesbarkeit (Tidy-Ausnahme
   dokumentieren, vgl. offene Tidy-Entscheidung aus Iter1).
4. **Variablen-Parität Rust↔C++ wird erzwungen, aber C++ erbt KEINE
   Safety von Rust** (eigener Abschnitt unten). Parität heißt:
   gleiche Filter-/Fehler-/Sortier-Semantik, gleiche Move-/Join-Disziplin,
   verifiziert durch Byte-Parität + Invarianten-Tests + Sanitizer, nicht
   durch Augenschein.
5. **Transpilat erst nach fertigem direktem C++.**
   `lisp/gen.lisp` (cl-cpp-generator2) emittiert den Core
   (Scanner/Color/Layout); GUI-Schale und `main` bleiben handgeschrieben.
   Generat wird wie fremder Code behandelt: erst nach Kompilat +
   `--scan`-Bytevergleich + Tests vertrauenswürdig.
6. **Skripte sind die Bedienoberfläche von iter2** (9 Dateien, s. Abschnitt
   Skripte). Jeder `task.md`-Schritt ruft sie auf; der Walkthrough belegt
   ihre Ausgaben.

## Variablen-Parität Und Safety (Antwort Auf Die Kernfrage)

Frage: „Geht C++ mit Variablen genauso um wie Rust? Erbt C++ daher
seine Sicherheit von Rust?" — Antwort: **Nein, kein Erbe.**
Der C++-Compiler beweist nichts davon; gleiche Struktur reduziert nur die
Review-Fläche. Parität wird deshalb pro Variablen-Klasse erzwungen
und geprüft:

| Klasse | Rust-Garantie (Compilezeit) | C++-Disziplin (Handarbeit) | Prüfung |
|---|---|---|---|
| Thread-Übergabe | Owned Values via Scope-Typ, kein `&mut`-Sharing | `std::move`, disjunkte `results[i]`-Slots, Join vor Weiterleben | Tests + ASan/UBSan; TSan-Arms empfohlen |
| Join | Scope joint automatisch | Join-Schleife im selben Scope, keine frühen `return`s zwischen Spawn und Join | Review-Checkliste + `fail`-Test bei Timeout |
| Use-after-move | Borrowchecker verbietet | Verbot per Review; keine Weiterverwendung nach `std::move` | `clang-tidy` (`misc-*`, `bugprone-*`) |
| Pfad-Lifetimes | `PathBuf`-Ownership klar | Kein `const fs::path&` an Iterator-Interna über Schleifenumbau hinweg | `bugprone-dangling-handle` Pflicht |
| Fehlerpfade | `Result` zwingt Behandlung | Jede `error_code`-Stelle prüfen + `report_skip`; Default-Weiterlauf verboten | Ungeprüfte Stellen per `grep`-Review + NOLINT nur begründet |
| Rekursionstiefe | gleiches Problem | gleiche Tiefe, Stack pro Thread beachten | Lasttest tiefer Baum |
| Sortierung/Index | Typ-Disjunktheit | `sort` nach Join, Index-Slots ohne Überlapp | Sortier-Invarianten-Test |

Der Vergleichsreport (`report_rust_cpp.md`) enthält diese Tabelle mit
Befund je Zeile plus Sanitizer-Protokoll. Ehrlicher Preis (aus Iter1):
Scope-Garantien, Move-Checks und Exhaustiveness ersetzt in C++ nur die
Kette Tests + Sanitizer + Review — teilweise, nicht beweisbar.

## Recommended Approach

1. Schritt 0: `git mv` nach `iter1/`, `iter2/`-Gerüst + Skript-Skelette,
   Fixture-Generator, Commit.
2. Rust seriell aus `gen`-freier Hand aufbauen (MVP-Logik + `report_skip` +
   `print_line`), Build/Fmt/Clippy grün.
3. Rust parallel (Scope, Subdir-Threads, Cap + `depth`, Re-Sort nach Join),
   Äquivalenz-Tests + `Instant`-Bench, Speedup-Tabelle.
4. Headless-/GUI-Smoke Rust (`xvfb`, Broken-Pipe, Filter-Kanten).
5. Direkten C++-Core nachbauen (Scanner/Color/Layout, `error_code` +
   Mutex-`report_skip`, Thread-Fanout mit Cap), `--scan`-Bytevergleich
   gegen Rust, `ctest` (ASan/UBSan), Bench-Parität, GUI-Schale + Smoke.
6. `lisp/gen.lisp` in kleinen Emitter-`defun`s schreiben (Backup- und
   Paren-Disziplin), Generat kompilieren, `--scan`-/Test-/Bench-Parität,
   Transpiler-Lücken dokumentieren.
7. `task.md`/`deps.md` final, Commits in Konvention,
   `walkthrough.md` + `report_rust_cpp.md` mit Messwerten, Safety-Befund,
   Learnings, Erweiterungen, Docker-Programmen.

## Work Plan

- Phase 0 — Umzug & Gerüst: `git mv`, `iter2/`-Skelett, 9 Skripte (Stubs),
  Fixture-Generator, `cargo search`-Newest-Protokoll.
- Phase A — Rust seriell: Typen, Scanner, Layout, Render/Utils, CLI
  (`--scan`/`--bench`), Build/Fmt/Clippy grün.
- Phase B — Rust parallel + Bench: Scope-Scan, Parallel-Layout,
  Äquivalenz-Tests, `Instant`-Bench (Median ≥ 5 Läufe, Fix-Fixture 4000).
- Phase C — Rust-Smoke: Headless-Tests, Broken-Pipe, Filter-Kanten,
  `xvfb`-Smoke seriell + parallel.
- Phase D — Direkter C++-Port: Core-Header, CMake (Sanitizer-Preset +
  Bench-Preset), `ctest`, Byte-Parität, Bench, GUI-Schale + Smoke.
- Phase E — Transpilat: Emitter-Module, Generierungslauf, Kompilat,
  Paritäts-Nachweise, Lücken-Liste mit Beispielen.
- Phase F — Dokumente & Commits: `deps.md` final, Konventions-Commits,
  `walkthrough.md` + `report_rust_cpp.md`.

## Skripte (Alle In `iter2/scripts/`, Pattern `setupNN_<name>.sh`)

| # | Datei | Funktion |
|---|---|---|
| 01 | `setup01_rust_build.sh` | Rust compilieren (Debug + Release) |
| 02 | `setup02_rust_test.sh` | Rust testen (`cargo test` + fmt + clippy) |
| 03 | `setup03_rust_bench.sh` | Rust benchmarken (`--bench`, Median-Tabelle) |
| 04 | `setup04_cpp_build.sh` | Direkten C++-Code compilieren (CMake, beide Presets) |
| 05 | `setup05_cpp_test_bench.sh` | Direkten C++-Code testen (`ctest`) + benchen |
| 06 | `setup06_transpile.sh` | C++-Transpilat erstellen (`sbcl` + `parenmedic diagnose`) |
| 07 | `setup07_transpiled_build.sh` | Transpilat compilieren (`-fsyntax-only` + CMake) |
| 08 | `setup08_transpiled_test.sh` | Transpilat testen (`ctest` + `--scan`-Diff) |
| 09 | `setup09_transpiled_bench.sh` | Transpilat benchmarken |

## Validation Plan

- `git log --oneline -- iter1 | head` zeigt den Umzugs-Commit; `iter1/`
  baut/testet unverändert.
- `cargo build`, `cargo fmt --check`,
  `cargo clippy --all-targets -- -D warnings` in `iter2/` grün.
- `cargo test` in `iter2/` grün (Layout-Invarianten, Scanner-Kanten,
  seriell-vs-parallel, Headless inkl. `| head -n 1`-Broken-Pipe).
- `cargo run --release -- --bench <fixture>`: Scan-/Layout-Zeiten +
  Speedup, Median ≥ 5 Läufe, Fixture fix (4000-Dateien-Baum + großer Baum).
- `cmake --build` (beide Presets), `ctest` grün; `diff <(rust --scan)`
  `<(cpp --scan)` leer; Bench-Tabelle Rust vs. C++ vorhanden.
- `sbcl --load lisp/gen.lisp --quit` Exit 0; `parenmedic diagnose`
  grün; `g++ -fsyntax-only` grün; Generat-Binary: `--scan`-Diff leer,
  Tests grün, Bench berichtet.
- `xvfb-run -a timeout 20 <bin> <dir>` → Exit 124, keine Panic-Stderr
  (C++-GUI nur unter `openbox`).
- `git status --short` → nur beabsichtigte Pfade, nie `target/`,
  `build/`, `/tmp`-Artefakte.

## Risks / Rollback

- Paren-Verlust im Lisp → `known-good`-Restore, kein Hand-Flicken.
- Transpiler-Form fehlt → kleinste semantikerhaltende Umformulierung oder
  dokumentierter String-Hatch (Walkthrough-Kandidat); notfalls Emitter-Modul
  handgepflegt als Ausnahme mit Begründung.
- C++-Bench-Divergenz (Stats/Fan-out) → d_type-Cache-Check + Cap-Check
  zuerst (Iter1-Befunde), kein Scheduler-Wechsel ohne Messung.
- PGE3-API-Drift → gegen vendored Header neu schreiben, Version pinnen.
- Rollback: `iter1/` ist je Commit revertierbar; `iter2/` additiv;
  Referenz-Repos unangetastet.

## Open Questions

1. Cushion-Shading übernehmen oder flach wie MVP? Default: flach,
   Cushion als Erweiterung.
2. `rayon`-Mess-Arm behalten oder verwerfen? Default: nur bei
   Speedup-Beleg, sonst verwerfen und im Walkthrough begründen.
3. 0-Byte-Filter (`size > 0`) übernehmen oder sichtbar machen?
   Default: übernehmen, Fix-Vorschlag in den Walkthrough.
4. Tidy-Naming (snake_case-Parität vs. Projekt-Config)? Default:
   Parität mit dokumentierter Ausnahme (Iter1-Befund: 0 echte Befunde,
   nur Naming + `misc-no-recursion`).

## Requirements-Abgleich Und Vorschlaege

Explizit gefordert und abgedeckt: `git mv` nach `iter1` + Neustart
`iter2`; Rust direkt → Test → Bench; semantisch äquivalenter C++-Code →
Test → Bench; Transpiler-Reimplementierung des C++-Codes; parallele
Version immer gegen serielle Referenz; Zwischen-Commits;
`walkthrough.md` + Rust-vs-C++-Report; nummerierte
`setup01_*.sh` … `setup09_*.sh`; `SUPPORTED_FORMS.md`-Referenz;
`parenmedic`-Disziplin (oft checken/fixen, Backups, ≤-60-Zeilen-Funktionen,
Closer-Stil); `xvfb`-Tests; kleine Code-/Dep-Basis mit Tools;
Architektur-Sicht; deepwiki-Doku + `deps.md` mit Orgs; neueste Versionen
trotz Kompatibilitätswarnung bei neuer Dep + Usage-Examples vorab;
Docker-Ubuntu-26-Installationsfreiheit; Feature-Vorschlag mit fehlenden
Requirements; Plan mit Dateiliste + Commit-Konvention; Unit-/Integration-
Tests + Ausführung; `task.md`; Abschluss-Walkthrough mit Learnings,
Erweiterungen, Docker-Programmen.

Zusätzlich vorgeschlagen (im Plan eingebaut): TSan als drittes
Sanitizer-Preset (nur Test, nie Bench); deterministischer
Fixture-Generator statt Handbaum; `--scan`-Bytevergleich als
automatisierter Test statt manuellem Diff; getrennte CMake-Presets
(sanitisiert vs. Bench); PGE3-Header-Version pinnen + vendorn;
Sortier-Invariante von Tag 1 (nicht als Bench-Fix); `depth`/Cap als
Parameter der ersten parallelen Version; Fehler-Helper vor Verteilen
der `?`-Stellen; Bench-Schwellen (Regression > 15 % → untersuchen).

## Kontext-Dateiliste Fuer Den Implementierungs-Agenten

- `plan/20260920_02_redo/prompt.txt` — Auftrag (diese Aufgabe).
- `plan/20260920_02_redo/{plan,task,deps}.md` — dieser Plan.
- `plan/20260919_01_merge/rust_cpp_vergleich.md` — Architektur-,
  Safety- und Portierungs-Lehren aus Iter1 (Pflichtlektüre).
- `plan/20260919_01_merge/walkthrough.md` — Messwerte, Befunde
  (Thread-Granularität, Stats, Fan-out, Hover), Transpiler-Lücken.
- `plan/20260919_01_merge/{plan,task,deps}.md` — Konventionen
  (Commits, Gates, Dep-Budget).
- `plan/20260919_01_merge/gen-cpp-freestanding-example.lisp` —
  freistehendes `write-source`-Minimalbeispiel + `lprint`-Vorbild.
- `iter1/src/main.rs` — direkte Rust-Vorlage (lesen, neu schreiben).
- `iter1/tests/headless_scan.rs` — Headless-/Broken-Pipe-Muster.
- `iter1/cpp/src/{010_scanner,015_color,020_layout}.hpp` — C++-Vorlage.
- `iter1/cpp/{gen.lisp,CMakeLists.txt,.clang-format,.clang-tidy}` —
  Transpiler-/Build-Vorlage.
- `/workspace/src/cl-cpp-generator2/SUPPORTED_FORMS.md` — verbindliche
  Lisp→C++-Formen (aus grünen Tests generiert).
- `/workspace/src/cl-cpp-generator2/example/197_shadertoy/gen4.lisp` —
  Strukturvorbild modularer Emitter.
- `/workspace/src/rs_disk_treemap/plan/20260918_02_review_and_xvfb_test/walkthrough.md` —
  xvfb-Smoke-Muster (Exit 124), EPIPE-Lehre, Docker-Pakete.
- `/workspace/src/rs_disk_treemap/00_mvp/src/main.rs` — Referenz-MVP
  (lesen, nicht ändern).
- `/workspace/src/pge_treemap/source0/src/` (`010`/`030`/`050`/`060`/
  `070`/`080`/`090_`) — GUI-Schalen-Kontrast.

## Commit-Konvention

Conventional Commits, Format `<type>(<scope>): <Imperativ>` + Body
(Was/Warum/Tests/ Messwerte bei Perf): Typen `feat`/`fix`/`perf`/
`test`/`docs`/`chore`/`refactor`; Scopes `iter2-rust`/`iter2-cpp`/
`iter2-lisp`/`iter2-scripts`/`plan`. Beispiele:
`feat(iter2-rust): serieller scan mit Instant-bench`,
`test(iter2-rust): seriell-vs-parallel aequivalenz`,
`feat(iter2-cpp): portiere scope-scan in DirectoryScanner`,
`feat(iter2-lisp): scanner-emitter mit known-good-backup`,
`docs(plan): walkthrough und rust-cpp-report`.
Ein logischer Schritt pro Commit, nur eigene Pfade
(`iter2/`, `plan/20260920_02_redo/`), nie `target/`/`build/`/`gen/`-Output.

## Deps (Newest-Check Bei Umsetzung, `cargo search` + `cargo info`)

Siehe `deps.md`: einzige Bestand-Dep `macroquad 0.4` (Newest-Check,
Lock-Abgleich, Usage-Example `Window::from_config`); mögliche neue Dep
nur mit Messbeleg + Usage-Example vorab; Tooling (`sbcl`,
`cl-cpp-generator2`, PGE3, System-Pakete) mit GitHub-Org und
deepwiki-Schlüssel. Bei Neueinführung: neueste Version nehmen (auch bei
Kompatibilitätswarnung), Beispiel in Plan + `deps.md` nachtragen.

## Task-Vorschau (Seriell, Details In `task.md`)

0. Umzug (`git mv`) + Gerüst + Skript-Stubs + Fixture-Generator.
1. Rust seriell (direkt), Build/Fmt/Clippy grün.
2. Rust parallel + Äquivalenz-Tests + Bench.
3. Rust-Smoke (Headless, Broken-Pipe, `xvfb`).
4. Direkter C++-Port + Byte-Parität + `ctest` + Bench + GUI-Smoke.
5. Transpiler-Input + Generat-Parität (Build/Test/Bench).
6. `deps.md` final, Konventions-Commits, `walkthrough.md` +
   `report_rust_cpp.md`.
