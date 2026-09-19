## Goal

Das Referenz-MVP (`00_mvp`, 286 Zeilen, `macroquad`, bewusst ohne Text im Treemap) wird zuerst in Rust parallelisiert (Scan + Layout), seriell gegen parallel auf Korrektheit und Speedup vermessen, danach wird genau diese parallele Architektur nach C++ (PGE3-GUI) portiert. Generiert wird der Rust-Code aus einer `gen.lisp`-Quelle per `sbcl` + Transpiler (Muster `197_shadertoy/gen4.lisp`), mit `parenmedic`-Disziplin, kleinen Funktionen (≤ 60 Zeilen) und minimalen Dependencies. Ergebnis in diesem Repo (`transpiled_treemap`): paralleler Rust-Prototyp mit Benchmark-Vergleich, C++-PGE3-Port, `gen.lisp`-Source-of-Truth, Tests, `task.md`/`deps.md`, und nach Abschluss `walkthrough.md` — alles in `plan/20260919_01_merge/`.

## Success Criteria

- Es existiert ein serieller Referenz-Stand (MVP-Logik, eingefroren/kopiert, nicht das fremde `00_mvp` überschreibend) und eine parallele Rust-Variante im selben Repo; beide bauen mit `cargo build` und sind `cargo fmt --check`-sauber sowie `cargo clippy --all-targets -- -D warnings`-frei.
- Korrektheits-Vergleich seriell vs. parallel ist automatisiert und grün: identische Größen-/Baum-Summen, Flächen-Erhalt (< 0,5 %), keine Geschwister-Überlappung, Sortier-Invariante — auf deterministischen Fixture-Bäumen plus einem Real-Verzeichnis.
- Benchmark-Harness (nur `std::time::Instant`, keine neue Dep) misst Scan- und Layout-Zeit seriell vs. parallel und druckt Speedup; Messung läuft reproduzierbar (festes Fixture, z. B. 4000 Dateien wie in `02`-Walkthrough: 17–77 ms-Basis im Debug-Build).
- C++-Port baut (`cmake --build`, Release ohne Sanitizer), nutzt PGE3 nur für die GUI-Schale und übernimmt die parallele Scan-/Layout-Architektur aus dem Rust-Prototyp; Rust-Safety-Konzepte sind abgebildet (s. Entscheidung 4).
- `gen.lisp` (Source of Truth) generiert per `sbcl`-Lauf ohne Fehler den Rust-Code (`sbcl --load gen.lisp --quit`, Exit 0); generierter Code ist committet und reviewbar; jede Emitter-`defun` ≤ 60 Zeilen, Top-Level-Closer auf eigenen Zeilen, `parenmedic diagnose` grün vor jedem Commit der Lisp-Datei.
- Unit-Tests (Layout-Invarianten, Scanner-Kanten: 0-Byte, Symlink, nicht-existent, `/proc`-Filter) und Integration-Tests (Headless-Scan inkl. Broken-Pipe-Fall nach `02`-Muster, seriell-vs-parallel-Äquivalenz) sind neu eingeführt und grün (`cargo test`).
- GUI-Smoke beider Varianten (Rust + C++) unter `xvfb-run -a timeout 20 …` mit Erfolg = `timeout`-Exit 124, kein Panic auf stderr (Muster `02`-Walkthrough).
- Commits folgen Conventional Commits (s. Abschnitt Commit-Konvention), nur eigene Pfade, nie `target/` oder generierte Build-Artefakte.
- `task.md`, `deps.md`, `walkthrough.md` liegen in `plan/20260919_01_merge/`; der Walkthrough enthält Messwerte, getroffene Annahmen-Korrekturen, Transpiler-Lücken mit Beispiel, Learnings, Erweiterungen und Docker-Programme.

## Context And Current Facts

- Referenz-MVP `/workspace/src/rs_disk_treemap/00_mvp/src/main.rs` (286 Zeilen, Freeze-Kommentar, Stand 18.09.): `Node { path/size/is_dir/children/rect/color }`, `scan_tree` (rekursiv, Sync-`fs::read_dir`, Symlink-Skip, `/proc|/sys|/dev`-Filter, `size > 0`-Filter, drei stille `Err`-Stellen), `squarify` (Bruls/Huizing/van Wijk, zwei Closures `worst`/`layout_row`, danach Rekursion in Dirs), `render_tree`, `color_for_path` (Extension-`match` + Hash-Fallback), `format_bytes`, `main` mit `#[macroquad::main]`, Bg-Thread + `channel`, Layout nur bei Resize. Einzige Dep `macroquad 0.4` (Lock 0.4.16).
- `01_more` (1440 Zeilen) ist NICHT Ziel: Watcher/Animation/Kamera/HUD bleiben draußen — Vorgabe „MVP gefällt besser, kurz, kein Text im Diagramm“.
- `pge_treemap/source0/src/` (2832 Zeilen total, Nummerierung `000_`–`090_`): `010_types.hpp` (`FileNode`, `SharedScanContext`, Double-Buffer-Snapshot), `030_thread_pool.hpp` (eigener `LayoutThreadPool`), `050_treemap_layout.hpp` (paralleles Fork/Join-Layout), `060_scanner.hpp` (Crawler + Snapshot-Publisher), `070_treemap_app.hpp` (Cushion-Renderer, LOD, HUD, Kamera), `080_cli.hpp`, `090_main.cpp` (PGE3-`Construct`/`Start`). Nur als GUI-Schale wiederverwenden; parallele Kernlogik kommt aus dem Rust-Prototyp (Auftragsvorgabe).
- Transpiler-Vorbild `cl-cpp-generator2/example/197_shadertoy/gen4.lisp`: modulare Emitter-`defun`s + `write-source`-Muster; freistehendes Minimalbeispiel `plan/20260919_01_merge/gen-cpp-freestanding-example.lisp` (59 Zeilen, `lprint`-`defun` mit `(:as "label" expr)`-Form, `write-source "output.cpp"` via `sbcl --load … -c quit --noinform`). Rust-Seite: Muster `rs_disk_treemap/03_mvp_lisp/gen.lisp` (`ql:register-local-projects` + `quickload "cl-rust-generator"`, `in-package`, `*code-file*`, `lprint` als ein `eprintln!` mit `{:?}`, `,@(loop …)`-Splices) und `04_more_lisp/lisp/*.lisp` (gesplittete Emitter-Module).
- Toolchain-Befunde dieser Sitzung: `sbcl` installiert, `parenmedic` unter `/workspace/src/parenmedic/zig-out/bin/parenmedic` (`diagnose`/`fix`), `rustc/cargo 1.98.1`, `cargo search rayon` → `1.12.0` (newest), `cargo search macroquad` → `0.4.16` (= Lock-Stand). `rayon` wurde in `01_more` bewusst wieder entfernt (tote Dep, `02`-Walkthrough) — Wiedereinführung hier nur mit Messbeleg.
- Verfahrene Lehren aus `02`-Walkthrough: `println!` + geschlossene Pipe = Panic → `print_line`-Helper mit `BrokenPipe`-Ausnahme; GUI-Smoke via `xvfb-run` + `timeout`-Exit 124; Container braucht `xvfb libxkbcommon0 libxi6 libx11-6 libgl1 libasound2t64`; `difft` als externer Diff beachten (`git diff --no-ext-diff` für Hunk-Splitting).
- deepwiki-MCP (`plops/cl-cpp-generator2`) ist in diesem Container ohne Transport nicht verfügbar (Beleglücke wie in `03`-`deps.md`); Ersatz: lokale `SUPPORTED_FORMS.md`, `run-tests.sh`-Suite, lokale Registry/`cargo search`/docs.

## Constraints And Non-goals

- Klein halten: keine neue Cargo-/CMake-Dep ohne Mess- oder Vereinfachungsbeleg; keine `criterion`/`tempfile`/`assert_cmd`-Dev-Deps (Harness via `std::time::Instant` + `std::process::Command`, Muster `01_more/tests/headless_scan.rs`).
- `00_mvp` und `01_more` bleiben unverändert (fremde Repos, Referenz nur gelesen/kopiert); `03_mvp_lisp`/`04_more_lisp` sind Vorgänger, kein Umbau.
- MVP-Charakter erhalten: kein Text im Treemap, keine Fonts, keine Kamera/Animation/Watcher/Headless-Optionen über das Test-Minimum hinaus.
- Non-goals: `notify`-Upgrade, CJK-Breiten, `HashMap`-Merge ohne Beleg, Release-Workflow-Entscheid — nur als Walkthrough-Erweiterungen notieren.

## Key Decisions

1. **Parallelisierung: `std::thread`-Scope + Kanal zuerst, `rayon` nur als Mess-Arm.** Scan ist I/O-lastig und rekursiv (pro Verzeichnis ein Task), Layout-Rekursion in Subdirs ist CPU-parallelisierbar, die Zeilen-Kernschleife bleibt sequenziell. `std::thread::scope` + `mpsc`-Kanäle reichen ohne neue Dep; `rayon 1.12.0` (newest, `rayon-core 1.13.0`) wird nur als optionaler Vergleichs-Arm (`--features parallel-rayon`) evaluiert und nur bei belegtem Speedup übernommen. Alternative „nur `rayon`“ verworfen: neue Dep ohne Beleg widerspricht der Klein-Vorgabe und der `02`-Lehre.
2. **Vergleich seriell vs. parallel als Test + Bench, nicht als Behauptung.** Gleicher Fixture-Baum → gleiche `size`-Summen und Layout-Invarianten (Fläche, Überlappung, Sortierung); Zeiten getrennt für Scan/Layout über `Instant`, mehrere Durchläufe, Median berichtet. Kein `criterion` (Dep-Budget).
3. **`gen.lisp` (cl-rust-generator) ist Source of Truth für den Rust-Stand, Muster `gen4.lisp`.** Struktur: Loader-`gen.lisp` + kleine Emitter-Module (je `defun` ≤ 60 Zeilen, Closer auf eigenen Zeilen), `write-source` mit `` `(do0 …) ``, `lprint`-`defun` (C++-Beispiel-Semantik: Ausdruck → Label via `emit-rs`, ein `eprintln!` pro Zeile), `,@(loop …)`-Splices gegen Wiederholung. Generiertes `main.rs` wird committet (Review ohne `sbcl` möglich). C++ wird NICHT transpiliert (Auftrag: C++-Port per Hand, nur GUI aus `pge_treemap`).
4. **Rust-Safety → C++-Abbildung (explizit, geprüft):** `Result`/`Option` → `std::optional`/`std::error_code`, kein `unwrap` auf heißem Pfad (Fehlerzähler + `report_skip`-Äquivalent); Borrow-Regeln → `const`-Korrektheit + `unique_ptr`-Ownership (`DeepCopyTree`-Muster aus `060_scanner.hpp` weiterverwenden); exhaustive `match` → `switch` mit `default`-Assert; Data-Races → `SharedScanContext`-Double-Buffer + `atomic` (aus `010_types.hpp`) statt Mutex um den Baum. `clippy`-Warnungen und `clang-tidy`/Sanitizer (ASan/UBSan wie in `pge_treemap`-CMake, Default ON) sind die jeweiligen Gates.
5. **Paren-Disziplin als Gate, nicht als Wunsch.** Vor jedem Lisp-Commit: `parenmedic diagnose` grün + `sbcl`-Generierungslauf grün; bei Paren-Fehler revert auf `known-good`-Backup (Auftragsvorgabe). Backup-Regel: vor jeder Lisp-Edit-Sitzung Kopie (`gen.lisp.known-good`).
6. **Dateiablage in DIESEM Repo:** `src/` (generierter + handgeschriebener Rust-Code getrennt: `src/main.rs` generiert, `src/bench.rs`/`src/parallel.rs` handgepflegt nur falls nicht emittierbar — dokumentierte Ausnahme), `gen.lisp` + `lisp/`-Emitter, `cpp/` (PGE3-Port, Nummerierung `000_`–`090_` wie `pge_treemap`), `plan/20260919_01_merge/{plan,task,deps,walkthrough}.md`. `prompt.txt` + C++-Beispiel bleiben committet.

## Recommended Approach

1. Spikes (nur `/tmp`): Transpiler-Proben (`thread::scope`-Closure, `mpsc`-Tupel-`let`, `format!`/`eprintln!`-Paren-Stil, `match`-auf-`Result`) je als Mini-`write-source` nach `/tmp`; parallel dazu Rust-Parallel-Skizze (Scope-Scan auf Fixture) zur Entscheidung `std` vs. `rayon`.
2. `gen.lisp`-Gerüst + serieller Referenz-Stand in diesem Repo aufbauen (MVP-Logik, `macroquad 0.4.16`), generieren → `cargo build` → `fmt`/`clippy` → Diff gegen `00_mvp` (informativ; CL-Nähe vor Texttreue, stille `Err`-Ignoranz wird durch EINE Helper-`fn` mit einem `eprintln!`-Ort ersetzt).
3. Parallele Variante aus derselben `gen.lisp`-Quelle (Feature-Gate oder zweites Binärziel), Äquivalenz-Tests + `Instant`-Bench, Speedup berichten.
4. C++-Port: `pge_treemap`-Schale kopieren (GUI/CLI/Color/Pool/Types), Scanner-/Layout-Kern aus dem Rust-Prototyp nachbauen, Sanitizer-Build + `xvfb`-Smoke.
5. Dokumente/Commits in `task.md`-Reihenfolge; `walkthrough.md` zuletzt mit Messwerten, Hatch-Liste, Transpiler-Lückenvorschlägen mit Beispiel, Docker-Programmen.

## Work Plan

- Phase A — Spikes & Baseline: Referenz lesend sichern; vier Transpiler-Proben + Scope-Scan-Probe nach `/tmp`; Entscheid `std`-only vs. `rayon`-Arm; `cargo search`-Newest-Check protokollieren.
- Phase B — `gen.lisp`-Gerüst + serieller Stand: Loader + Emitter-Module (§1 Traversal, §2 Layout, §3 Render/Utils, §4 Main-Loop), `lprint` + `report_skip`-Helper, erster Generierungslauf, Build/Fmt/Clippy grün.
- Phase C — Parallele Variante: Scan-Tasks + Layout-Fork/Join, Äquivalenz-Unit-Tests (Summen, Fläche, Überlappung, Sortierung), `Instant`-Bench seriell vs. parallel (Median, fester Fixture).
- Phase D — Integration & GUI-Smoke: Headless-Tests (inkl. Broken-Pipe, `/proc`-Filter, Symlink, 0-Byte), `xvfb`-Smoke Rust (seriell + parallel).
- Phase E — C++-Port: Schale übernehmen, Kern portieren, CMake-Build (ASan/UBSan), `clang-format`-nahe Ordnung, `xvfb`-Smoke C++.
- Phase F — Dokumente & Commits: `task.md` (diese Phasen seriell mit Gates), `deps.md` (Tabelle mit GitHub-Org + deepwiki-Schlüssel + Usage-Example bei neuer Dep), Commits in Konvention, `walkthrough.md` mit Messwerten/Learnings/Erweiterungen/Docker-Programmen.

## Validation Plan

- `sbcl --load gen.lisp --quit` → Exit 0, Ziel-`main.rs` geschrieben-Meldung; `parenmedic diagnose gen.lisp lisp/` → grün.
- `cargo build`, `cargo fmt --check`, `cargo clippy --all-targets -- -D warnings` → grün (beide Rust-Ziele).
- `cargo test` → grün: Layout-Invarianten, Scanner-Kanten, seriell-vs-parallel-Äquivalenz, Headless inkl. `| head -n 1`-Broken-Pipe-Fall (Exit 0, kein `panicked`).
- Bench: `cargo run --release -- --bench <fixture>` druckt Scan-/Layout-Zeiten + Speedup; Median über ≥ 5 Läufe, Fixture fix (4000-Dateien-Baum).
- `xvfb-run -a timeout 20 ./target/debug/<bin> <dir>` (Rust seriell + parallel) und C++-Binary → jeweils Exit 124, leere Panic-Stderr (Höchstrisiko-Schritt: C++-GUI-Smoke fängt Port-Divergenzen).
- `sh /workspace/src/cl-rust-generator/run-tests.sh` → grün (keine Transpiler-Regression durch neue Formen).
- `git status --short` → nur beabsichtigte Pfade, keine `target/`-/`build/`-Artefakte.

## Risks / Rollback

- `thread::scope`-Closure nicht emittierbar → String-Escape-Hatch (kommentiert, Walkthrough-Kandidat) oder kleinste semantikerhaltende Umformulierung; notfalls parallele Variante handgepflegt neben generiertem Serien-Stand (dokumentierte Ausnahme).
- `rayon`-Arm bringt keinen Speedup (I/O-bound) → Arm verwerfen, `std`-only bleibt; kein Dep-Rückstand (Feature-Gate, Lockfile-Diff sichtbar).
- PGE3-Build im Container scheitert (X11/GL-Libs) → Pakete aus `02`-Walkthrough nachinstallieren (`xvfb libxkbcommon0 libxi6 libx11-6 libgl1 libasound2t64` + `-dev`-Build-Pendants); kein GUI-Rateversuch ohne `xvfb`.
- Paren-Verlust → `known-good`-Backup-Restore, kein Hand-Flicken langer Formen.
- Rollback: alles neu in diesem Repo, Referenz-Repos unangetastet; jedes Commit einzeln revertierbar; C++-Port additiv unter `cpp/`.

## Open Questions

1. C++-Port ebenfalls `macroquad`-äquivalente Cushion-Shading-Stufe aus `070_treemap_app.hpp` übernehmen oder bewusst flach wie MVP (kein Farbverlauf)? Default: flach wie MVP, Cushion als Walkthrough-Erweiterung.
2. `rayon`-Vergleichs-Arm als permanentes Feature-Gate behalten oder nach Messung entfernen? Default: nach Messung entfernen, Ergebnis im Walkthrough.
3. 0-Byte-Dateien weiter unsichtbar (`size > 0`-Filter aus MVP) oder sichtbar machen? Default: Filter übernehmen, Fix-Vorschlag in den Walkthrough.

## Requirements-Abgleich Und Vorschlaege

Explizit gefordert und abgedeckt: Parallel-/Async-Speedup des MVP, seriell-vs-parallel-Vergleich, Rust→C++-Port mit PGE3 (nur GUI aus `pge_treemap`, Kern aus Rust-Prototyp), `gen.lisp` nach `gen4.lisp`-Muster + `sbcl`-Lauf, `SUPPORTED_FORMS.md`-Referenz, freistehendes C++-Beispiel als Vorlage, Paren-Disziplin (`parenmedic`, ≤-60-Zeilen, Closer-Stil, Backups), `xvfb`-Tests, kleine Code-/Dep-Basis mit Tools, Architektur-Sicht, deepwiki-Doku + `deps.md` mit Orgs, neueste Versionen + Usage-Examples, Feature-Vorschlag, Plan mit Dateiliste + Commit-Konvention, Unit-/Integration-Tests + Ausführung, `task.md`, `walkthrough.md` + Docker-Programme.

Zusaetzlich vorgeschlagen (im Plan eingebaut): `std::thread::scope`-First statt sofort `rayon` (Dep-Budget + `02`-Lehre); faktorisierte `Err`-Meldung statt stiller Ignoranz (eine Helper-`fn`, Einzeiler pro Stelle); `Instant`-Bench ohne neue Dep; Broken-Pipe-Helper von Anfang an; `known-good`-Backup-Regel; C++-Safety-Abbildung explizit prüfbar (Sanitizer-Gate); Cushion-Shading bewusst als Erweiterung, nicht als Default.

## Kontext-Dateiliste Fuer Den Implementierungs-Agenten

- `plan/20260919_01_merge/prompt.txt` — Auftrag (diese Aufgabe).
- `plan/20260919_01_merge/gen-cpp-freestanding-example.lisp` — freistehendes `write-source`-Minimalbeispiel + `lprint`-Vorbild.
- `/workspace/src/rs_disk_treemap/00_mvp/src/main.rs` — Referenz-MVP (Traversal/Layout/Render/Main-Loop, 286 Zeilen; lesen, nicht ändern).
- `/workspace/src/rs_disk_treemap/plan/20260918_02_review_and_xvfb_test/walkthrough.md` — `xvfb`-Smoke-Muster (Exit 124), EPIPE-Lehre, Docker-Pakete.
- `/workspace/src/rs_disk_treemap/plan/20260918_03_transpile_mvp/{plan,task,deps}.md` — Transpiler-Konvention (`gen.lisp`-Muster, Hatch-Budget, CI-Eintrag).
- `/workspace/src/rs_disk_treemap/03_mvp_lisp/gen.lisp` — Loader-Muster (`register-local-projects`, `*code-file*`, `lprint`, Splices).
- `/workspace/src/pge_treemap/source0/src/010_types.hpp`, `030_thread_pool.hpp`, `050_treemap_layout.hpp`, `060_scanner.hpp`, `070_treemap_app.hpp`, `080_cli.hpp`, `090_main.cpp` — C++-GUI-Schale + vorhandener Parallel-Layout/Scanner als Kontrast zum Rust-Kern.
- `/workspace/src/cl-cpp-generator2/SUPPORTED_FORMS.md` — belegte Lisp→C++-Formen (generiert aus grünen Tests).
- `/workspace/src/cl-cpp-generator2/example/197_shadertoy/gen4.lisp` — Strukturvorbild für modulare Emitter.
- `/workspace/src/rs_disk_treemap/01_more/src/main.rs` (Auszüge) — Negativ-Vorbild: zu groß für MVP-Scope; nur Headless-/Broken-Pipe-Muster übernehmen.

## Commit-Konvention

Conventional Commits, Format `<type>(<scope>): <Imperativ>` + Body (Was/Warum/Tests): Typen `feat`/`fix`/`test`/`docs`/`chore`/`refactor`; Scopes `rust`/`cpp`/`lisp`/`plan`; Beispiele: `feat(rust): parallel scope-scan mit Instant-bench`, `test(rust): seriell-vs-parallel aequivalenz`, `feat(cpp): portiere scope-scan in DirectoryScanner`, `docs(plan): task deps walkthrough`. Ein logischer Schritt pro Commit, nur eigene Pfade, nie `target/`/`build/`.

## Deps (Newest-Check Stand Heute, `cargo search`)

| Crate / Tool | Stand | Verwendet in | GitHub-Org/Repo | deepwiki-Vorschlag | Usage-Example im Plan |
|---|---|---|---|---|---|
| `macroquad 0.4` (Lock-Ziel 0.4.16) | newest = Lock | Rust seriell + parallel (Fenster/Text/Input) | `not-fl3/macroquad` | `not-fl3/macroquad` | keines nötig (Bestand, kein Wechsel) |
| `rayon 1.12.0` (`rayon-core 1.13.0`) | newest | NUR optionaler Mess-Arm (`--features parallel-rayon`), Übernahme nur bei Speedup-Beleg | `rayon-rs/rayon` | `rayon-rs/rayon` | `use rayon::prelude::*;` + `par_bridge()` im Scan-Arm; Beispiel + Messwert bei Einführung in `deps.md` nachtragen |
| `sbcl` + `cl-rust-generator` (Quicklisp-Local) | Tooling, keine Cargo-Dep | `gen.lisp`-Generierung | `plops/cl-rust-generator` | `plops/cl-rust-generator` | `(ql:quickload "cl-rust-generator")` nach `(ql:register-local-projects)` (Muster `03`-`gen.lisp`) |
| `cl-cpp-generator2` | Referenz/Docs | Formen-Referenz (`SUPPORTED_FORMS.md`), kein Generator dieses Tasks | `plops/cl-cpp-generator2` | `plops/cl-cpp-generator2` | `sbcl --load gen-cpp-freestanding-example.lisp --quit` schreibt `output.cpp` |
| PGE3 (`olcPixelGameEngine3`) | GUI-Schale C++ | `cpp/`-Port (Render/CLI) | `OneLoneCoder/olcPixelGameEngine3` | `OneLoneCoder/olcPixelGameEngine3` | `demo.Construct(config)` / `demo.Start()` (Muster `090_main.cpp`) |
| System (Docker): `xvfb libxkbcommon0 libxi6 libx11-6 libgl1 libasound2t64` (+ `-dev`-Build-Pendants) | nachinstallieren falls fehlend | GUI-Smoke/Build | — (Ubuntu 26.04) | — | `xvfb-run -a timeout 20 ./<bin> <dir>; echo $?` → 124 erwartet |

Kein `criterion`/`tempfile`/`assert_cmd` (Budget-Regel); einzige mögliche neue Cargo-Dep ist `rayon` — und nur mit Messbeleg.

## Task-Vorschau (Seriell, `task.md` Wird Bei Umsetzung Angelegt)

0. Baseline: Referenz lesen, `cargo search`-Stände notieren, Spikes nach `/tmp`, `git status`-Hygiene.
1. Gerüst: `gen.lisp` + Emitter-Skelett, serieller Stand, Generierung grün, Build/Fmt/Clippy grün.
2. Parallel-Arm: Scope-Scan + Layout-Fork/Join, Äquivalenz-Tests, `Instant`-Bench mit Speedup-Tabelle.
3. Integration/GUI-Smoke (Rust): Headless-Tests, Broken-Pipe, `xvfb`-Smoke seriell + parallel.
4. C++-Port: Schale + Kern-Port, Sanitizer-Build, `xvfb`-Smoke.
5. Transpiler-Regression (`run-tests.sh`), `task.md`/`deps.md` final, Commits in Konvention, `walkthrough.md` mit Messwerten/Learnings/Erweiterungen/Docker-Programmen.

## Walkthrough-Vereinbarung

Nach vollständigem Grün (alle Tests + Smokes + Commits) schreibt der Umsetzungs-Agent `plan/20260919_01_merge/walkthrough.md`: was wirklich implementiert wurde, testbedingte Änderungen mit Messwerten, `std`-vs-`rayon`-Entscheid mit Zahlen, Transpiler-Lücken mit Beispiel-Anwendung, umgesetzte Verbesserungen, Learnings, Erweiterungen (Cushion, `rayon`-Gate, Headless-Optionen), neue Docker-Programme.
