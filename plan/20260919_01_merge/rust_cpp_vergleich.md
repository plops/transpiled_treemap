# Rust ↔ C++ im Vergleich: Architektur, Portierung, Safety-Lücken, Learnings

Stand: 2026-09-19 · Code: `src/main.rs` + `tests/headless_scan.rs` (Rust),
`cpp/src/`, `cpp/tests/`, `cpp/gen.lisp` (C++). Sprache: Deutsch.

## 1. Die Rust-Architektur

Der gesamte Prototyp lebt in einer Datei (`src/main.rs`, ca. 600 Zeilen
inkl. Tests) plus einem Black-Box-Integrationstest. Bausteine:

- **Daten:** `struct Node { path: PathBuf, size: u64, is_dir: bool,
  children: Vec<Node>, rect: Rect, color: Color }` — ein Baum aus
  *Owned Values*, keine Referenzen, keine Lebensdauer-Annotationen nötig.
- **Serieller Pfad (Referenz):** `scan_tree` / `scan_entry` (rekursives
  `fs::read_dir`, Symlink-Skip, `/proc|/sys|/dev`-Filter, `size > 0`-Filter),
  `worst` / `layout_row` / `squarify` (Bruls/Huizing/van Wijk, Zeilenpass
  plus Rekursion), `render_tree`, `color_for_path`, `format_bytes`.
- **Paralleler Pfad:** `scan_tree_parallel_impl(path, depth)` und
  `squarify_parallel_depth(nodes, rect, depth)` — gleiche Algorithmen,
  aber Subtrees wandern als Owned Values in `std::thread::scope`-Threads.
  Dateien bleiben inline (Thread-pro-Datei war 12x langsamer), die
  Fan-out-Tiefe ist gedeckelt (atomarer Zähler, 4×nCPU; Layout nur
  Top-Level). Nach dem Join wird die Größenordnung wiederhergestellt
  (`sort_unstable_by_key(Reverse)`), weil Threads in
  Fertigstellungsreihenfolge zurückkommen.
- **Fehler:** `Result`-Stellen melden über genau eine Helper-Funktion
  (`report_skip`, ein `eprintln!`-Ort); `print_line` überlebt geschlossene
  Pipes (`BrokenPipe`-Ausnahme). Kein `unwrap` auf E/A-Pfaden.
- **Warum das trägt:** Der Borrowchecker beweist zur Compilezeit, dass
  (a) kein Thread ein `&mut` mit einem anderen teilt, (b) `scope`-Handles
  vor dem Zusammenführen gejoint sind, (c) nach `join().unwrap_or`/Match
  kein uninitialisierter Wert weiterlebt. `cargo clippy -D warnings`
  ist grün.

## 2. Die C++-Umsetzung derselben Architektur

`cpp/src/010_scanner.hpp` (Typen + Scanner), `015_color.hpp` (Palette),
`020_layout.hpp` (Layout), `090_main.cpp` (--scan/--bench),
`030_pge_app.hpp` + `095_gui_main.cpp` (PGE3-Schale), dazu
`cpp/gen.lisp` als Transpiler-Quelle der drei Core-Header. Jede
Rust-Funktion hat ein 1:1-Gegenstück mit snake_case-Namen, damit man
beide Dateien nebeneinander lesen kann. Verifikation: `--scan`
byte-identisch, `--bench` auf Parität, `ctest` grün (inkl. ASan/UBSan),
GUI-Smoke mit Screenshot.

## 3. Was anders gemacht werden musste

- **Kein `thread::scope`:** C++20 kennt keinen Scoped-Thread. Ersatz:
  `std::vector<std::thread>` + `join()`-Schleife im selben Scope, dazu
  ein atomarer Zähler (`ScanWorkersActive`) als Budget — in Rust erledigt
  das der Scope-Typ implizit, in C++ ist es handgeschriebene Disziplin.
- **Kein `Result`:** `std::error_code`-Out-Parameter an jeder
  Filesystem-Stelle; jede Stelle prüft und meldet per `report_skip`
  (stderr unter Mutex). Vergisst man eine Prüfung, läuft der Code mit
  Default-Werten weiter statt zu stoppen.
- **Kein Borrowchecker:** Owned-Subtrees per `std::move` über
  Thread-Grenzen; disjunkte `results[i]`-Slots statt Slices. Der Compiler
  prüft davon nichts — die Korrektheit steckt in der Join-Reihenfolge.
- **Dateityp-Abfrage:** Rusts `file_type()` kostet dank `d_type` null
  Syscalls; naives C++ (`status()` + `symlink_status()`) kostete 3 Stats
  pro Datei (5,6x langsamer). Fix: `is_symlink/is_directory/
  is_regular_file` (libstdc++-Cache) → 1 Stat/Datei, Parität.
- **Layout-Tiefe:** Unbegrenzter Fan-out war 5x langsamer als seriell —
  deshalb `depth`-Parameter in beiden Sprachen (Top-Level-only).
- **Strings/Format:** `format!` → `snprintf`-Idiom; `PathBuf` →
  `fs::path`; `Color`/`Rect` als POD-Structs.
- **PGE3-API:** Die aktuelle Main-Branch-API (`draw.FilledRect/Rect/
  String`, `GetMouse()`, `ScreenSize()`) unterscheidet sich von der,
  gegen die `pge_treemap` geschrieben wurde — die Schale musste gegen
  den vendored Header neu geschrieben werden.

## 4. Was in C++ deutlich unsicherer ist (nur zur Laufzeit sichtbar)

1. **Disjunkte Slot-Writes** (`results[i]`, `taken[dirIdx[k]]`): bei
   Indexfehlern Data-Race/UB — weder Compiler noch Tests finden das
   zuverlässig, nur TSan oder ein Crash in Produktion. In Rust ist die
   analoge Konstruktion per Typ beweisbar disjunkt.
2. **Join-Disziplin:** Vergessene `join()`s, Exceptions zwischen
   `emplace_back` und Join oder frühe `return`s lassen Threads auf
   zerstörte Locals laufen (`std::terminate` bzw. UB). Rusts Scope
   joint automatisch.
3. **Use-after-move:** `std::move` ist nur ein Cast; versehentliche
   Weiterverwendung kompiliert und ist UB. Clippy/Rust verbieten das.
4. **Rekursionstiefe:** Tiefe Bäume fressen Stack pro Thread (8 MiB
   Default); Rust hat dasselbe Problem, aber C++-Threads mit kleinem
   Stack (oder zur Laufzeit) fallen später und härter.
5. **Pfad-Lifetimes:** `const fs::path& p = entry.path();` bindet an
   Interna des Iterators — legal, aber ein Refactoring der Schleife
   kann daraus ein Dangling machen. `bugprone-dangling-handle` ist in
   der Projekt-Tidy-Config deshalb Pflicht.
6. **Stille Fehlerpfade:** Ungeprüfte `error_code`s kompilieren; der
   einzige Schutz ist Review + die beiden NOLINT-begründeten Stellen.
7. **Sanitizer-Pflicht:** ASan/UBSan sind 3–5x langsamer, aber ohne sie
   sind die Punkte 1–4 praktisch unsichtbar. Bench-Zahlen gelten nur
   für sanitize-freie Builds; Tests laufen immer mit Sanitizern.

## 5. Learnings: Rust-Code port-freundlich designen (geht das?)

Teils ja — die teuren Unterschiede lassen sich durch Stil vorwegnehmen:

- **Owned-über-Threads als Regel:** Schon in Rust nur Owned Values
  (nie `&mut`-Slices) über Thread-Grenzen geben; dann ist der C++-Port
  mechanisch (`std::move` statt Ownership-Transfer umdenken).
- **Tiefe/Budget als Parameter von Anfang an:** `depth` und ein Cap
  gehören in die erste parallele Version, nicht als Bench-Fix hinterher
  — in C++ nachrüsten ist fehleranfällig (atomare Zähler, Join-Pfade).
- **Fehler als Werte, zentral gemeldet:** Eine einzige
  Report-Funktion pro Bereich portiert 1:1 auf `error_code`-Prüfungen;
  verstreute `?`-Stellen laden in C++ zum Vergessen ein.
- **Index-basierte Parallel-Slots statt Iterator-Zucker:**
  `results[i]`-Muster in Rust vorwegnehmen (statt z.B. `par_bridge`),
  damit der C++-Join dieselbe Form hat.
- **Keine Borrow-Cleverness an Thread-Grenzen:** `split_at_mut`,
  Reborrows und Closures mit Captures sind in Rust elegant, in C++
  nicht abbildbar. Explizite `take`/Move-Blöcke lesen sich in beiden
  Sprachen gleich.
- **Namen stabil halten:** snake_case in C++ bricht die Projekt-Tidy-
  Config (s.u.), erhält aber die 1:1-Lesbarkeit. Diese Entscheidung
  gehört dokumentiert, nicht erschlichen.
- **Nicht portierbar:** Scope-Garantien, Move-Semantik-Checks und
  Exhaustiveness bleiben Compiler-leistung — in C++ ersetzen Tests +
  Sanitizer + Review sie nur teilweise. Der ehrliche Preis steht in
  Abschnitt 4.

## 6. Offene Entscheidung: Projekt-Tidy-Config vs. Rust-Parität

`cpp/.clang-tidy` (deine Config) fordert u.a. camelBack-Funktionen,
CamelCase-Structs und `m_`-Member. Lauf gegen den aktuellen Stand:
**0 echte Befunde** (kein bugprone/Analyzer/Performance-Problem) —
ausschließlich Naming-Verstöße (ca. 30, alle snake_case aus Rust-Parität)
und `misc-no-recursion` (Rekursion ist das Design, wie in Rust).
Ich habe nichts umbenannt: Ein Rename wäre semantikerhaltend, würde aber
die 1:1-Lesbarkeit Rust↔C++ zerstören, auf der Reviews, Walkthrough und
`gen.lisp` aufbauen. Optionen: (a) snake_case per Config-Ausnahme für
den Core erlauben, (b) alles auf camelBack umbenennen (großes Diff,
`gen.lisp`-Emitter müssen mit), (c) Status quo mit NOLINT-Datei.
Bitte entscheiden — danach ist `clang-tidy` mit Projekt-Config grün.

## 7. Repro-Kommandos

- `cargo test`, `cargo clippy --all-targets -- -D warnings`
- `./target/release/treemap_parallel --bench /workspace/src`
- `cmake --build /tmp/tm-build` (isoliert; `cpp/build/` gehört einer
  parallelen Session mit fremden Pfaden — nicht wiederverwenden),
  `ctest`, `g++ -O2`-Binary für Bench (sanitize-frei messen!)
- GUI: Xvfb + `openbox` (ohne WM: `_NET_WM_STATE`-BadAtom, Host-Level),
  `xdotool mousemove` + `xwd` für Hover-Beweis.
