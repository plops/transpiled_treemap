# Walkthrough: paralleler Treemap (Rust direkt + C++-Port + C++-Transpiler-Input)

Datum: 2026-09-19 · Stand: implementiert, verifiziert, uncommitted.
Richtungsentscheidung von wol pumba während der Umsetzung: Rust und C++
**direkt** schreiben (kein Rust-Transpiler); Transpiler-Input erst am Ende
und nur für den fertigen C++-Code (`cpp/gen.lisp`, cl-cpp-generator2).

## Was wirklich implementiert wurde

1. **Rust (`src/main.rs`, `tests/headless_scan.rs`)**: serieller
   Referenz-Scan + `squarify` (MVP-Logik) plus parallele Variante
   (`std::thread::scope`, Owned-Subtrees, keine neue Dep). `--scan`
   (eine Zeile, BrokenPipe-sicher) und `--bench` (5 Runden seriell vs.
   parallel) laufen ohne Fenster (plain `main` + `Window::from_config`,
   Muster `01_more`); GUI nutzt Parallel-Scan im Bg-Thread und
   `squarify_parallel` bei Resize.
2. **C++ (`cpp/src/`)**: `010_scanner.hpp`, `015_color.hpp`,
   `020_layout.hpp` (PGE-frei, Architektur = Rust-Prototyp:
   Fehler via `report_skip`+`error_code`, `unique`-Ownership via Moves,
   Joins geprüft), `090_main.cpp` (--scan/--bench),
   `030_pge_app.hpp` (PGE3-Schale, `#ifdef TREEMAP_HAS_PGE3`),
   `cpp/tests/core_tests.cpp`, `cpp/CMakeLists.txt` (ASan/UBSan default ON).
3. **Transpiler-Input (`cpp/gen.lisp` → `cpp/gen/*.hpp`)**: Emitter-`defun`s,
   `lprint`-Helper, String-Hatches mit Präzedenz. Generat kompiliert
   (`g++ -fsyntax-only`) und verhält sich identisch (--scan byte-gleich,
   --bench grün, Missing-Dir sicher).

## Messungen (Fixture 4000 Dateien, 40×100×1 KiB)

| Implementierung | seriell Scan | parallel Scan | Speedup |
|---|---|---|---|
| Rust (`--release`) | ~7–10 ms | ~1–2 ms | ~4–5x |
| C++ (CMake Release) | ~45 ms | ~10–11 ms | ~4,2–4,6x |
| C++ generiert (`-O2`) | ~19 ms | ~2 ms | ~8–10x (warmer Cache) |

Layout-Zeiten: < 1 ms (Fixture zu klein für Layout-Speedup; Parallel-Layout
ist Korrektheits-parität, kein Speed-Claim). Äquivalenz: Größen identisch,
Fläche < 0,5 %, Sortierung wiederhergestellt (s.u.).

## Nachtrag 2: Hover-Fix + C++-Speedup (großer Baum, 100k Dateien)

1. **Hover**: `root.rect` wurde im GUI-Worker nie gesetzt → Hit-Test
   verwarf alles. Fix eine Zeile (`root.rect = canvas`), `find_hover`
   public + Unit-Tests (`core_tests`: rechts/links/daneben). Per
   `xdotool`-Screenshot belegt: Header zeigt
   `/tmp/bench_tree/d11/f051.bin (1.0 KB)` unter dem Cursor.
2. **C++-Tempo**: Repro `/workspace/src`: Rust 0,39 s vs. C++ 2,19 s.
   Ursachen: 3 Stats pro Datei (`status` + `symlink_status` + `size`)
   und unbegrenzte Thread-Erzeugung. Fixes: `is_symlink/is_directory/
   is_regular_file` (d_type-Cache → 1 Stat/Datei, strace-belegt 13.421 →
   6.264), Fan-out-Cap (4×nCPU, atomar) für Scan, Top-Level-only für
   Layout (unbegrenzt war 5x langsamer als seriell). Cap auch in Rust
   nachgezogen (0,97x → 1,4x warm).
3. **Endstand warm-cache** (`/workspace/src`): C++ seriell 0,57 /
   parallel 0,24 s (2,4x); Rust seriell 0,36 / parallel 0,26 s (1,4x);
   Layout jeweils ~0,006 → ~0,003 s (2x). C++ seriell bleibt ~1,5x
   hinter Rust (`std::filesystem`-Overhead); auf dem parallelen Pfad
   (den die GUI nutzt) Parität. Sanitizer-Builds sind 3–5x langsamer —
   Bench-Zahlen gelten für sanitize-freie Builds.

## Testbedingte Änderungen (Befunde mit Fix)

- **Thread-pro-Datei war 12x langsamer** (0.009 s → 0.108 s): Fix —
  Dateien inline, Threads nur pro Subdirectory (Rust + C++ identisch).
- **Join zerstört Sortierordnung**: parallele Reassemblierung ist
  Fertigstellungs-Reihenfolge; Fix — `sort_unstable_by_key(Reverse(size))`
  bzw. `std::sort` nach Join (Rects wandern mit). Test
  `serial_parallel_layout_invariants` fing das.
- **`g++`-Erstbuild**: Lambda-Capture `[&]` ohne `h` (`015_color.hpp`) —
  trivialer Fix.
- **GUI-Smoke**: erst `libxkbcommon`-Panic, nach `apt-get install
  libxkbcommon0`: Exit 124, kein Panic.

## Transpiler-Lücken mit Beispiel (Upstream-Vorschläge cl-cpp-generator2)

1. `(dot p (string))` → `p."nil"` (`string` ist Literal-Op). Vorschlag:
   Null-arg-Methodenform, z.B. `(dot p (string ()))` → `p.string()`.
   Workaround: Hatch `"p.string ()"`.
2. `(+= a b)` → `+=(a, b)`. Gewusst-wie: `(incf a b)` → `(a)+=(b)`.
   Vorschlag: `+=` als binäres Op aufnehmen.
3. `(or …)` → `|`, `(and …)` → `&`. Gewusst-wie: `logior`/`logand`.
   Vorschlag: Doku-Hinweis oder `or`→`||`-Mapping.
4. `(let ((x Node)))` → `auto x = Node` (ungültig). Gewusst-wie:
   `(let ((x (Node))))` → `auto x = Node()`.
5. `(let ((v "std::vector<T>")))` → ohne `()`. Gewusst-wie: Typ als Call
   schreiben (`("std::vector<T>")`).
6. `declare (mutable …)` wird wörtlich emittiert (Rust-Form). Vorschlag:
   ignorieren oder `const`-Gegenstück.
7. `0.0`-Literal → `0.F`. Gewusst-wie: immer `0.0f` schreiben.
8. `(if …)` als Ausdruck → `auto x = if (…)`. Gewusst-wie: `?`-Op.
9. `(bracket …)` → `[...]` (kein gültiges Init). Gewusst-wie: `curly`.
10. `for` ist C-style `(start end iter)`; Range-for heißt `foreach`.
11. Methoden-Call statt Member: `(dot nodes (sort …))` →
    `nodes.sort(…)`; Freifunktion via `(std--sort …)`.

## Learnings

- Granularität schlägt Scheduler: Thread-pro-Verzeichnis + Owned-Moves
  reicht für ~5x; `rayon` wäre totes Gewicht gewesen.
- `sbcl`-Reader als Paren-Orakel: Top-Level-Formen einzeln lesen findet
  die defekte Stelle schneller als `parenmedic`-Kaskaden (s. gen.lisp:
  Lese-Fehler bei Form 12 → `progn` isoliert → String-Lexer-Bilanz).
- Generierter C++-Code ist erst nach Kompilat + Verhaltensvergleich
  (scan/bench) vertrauenswürdig, nicht nach Augenschein.
- `Window::from_config`-Muster (statt `#[macroquad::main]`) hält
  --scan/--bench fensterlos und xvfb-frei.

## Nachtrag: PGE3-Header vendored, GUI verifiziert

`cpp/third_party/olcPixelGameEngine3.h` (790.014 Bytes, main-Branch) per
`curl` geladen; `030_pge_app.hpp` gegen die echte API neu geschrieben
(`draw.FilledRect/Rect/String`, `GetMouse()`, `ScreenSize()`;
`pge_treemap` nutzt eine ältere API ohne diese Namen). CMake-Target
`treemap_gui` (X11 + Xi + GL + PNG — PNG nur gelinkt weil der Header-Loader
es referenziert, App lädt keine Bilder). Befund: unter nacktem `xvfb`
stirbt PGE3 beim ersten Expose (`_NET_WM_STATE`-Atom ohne WM → BadAtom,
Exit 1, Host-Level, vor App-Code); mit `openbox` läuft die GUI
(Exit 124, leere stderr, Screenshot belegt Treemap + HUD bei 49 FPS).

## Mögliche Erweiterungen

Cushion-Shading (flach wie MVP geblieben); `rayon`-Arm nur bei Beleg;
CJK-Breiten; `gen.lisp` auf `090_main.cpp`/`030_pge_app.hpp` ausweiten
(derzeit Core-Header).

## Neue Programme für den Docker-Container

`xvfb`, `libxkbcommon0`, `libxi6`, `libx11-6`, `libgl1` (Laufzeit);
Build: `libx11-dev libxi-dev libgl1-mesa-dev libasound2-dev cmake g++ sbcl`.
Installiert: `apt-get install -y xvfb libx11-dev libxi-dev
libgl1-mesa-dev libasound2-dev libxkbcommon0`.
