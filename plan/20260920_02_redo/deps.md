# deps.md — Abhängigkeiten (20260920_02_redo, Stand 2026-09-20, Planung)

Vorgabe: Code und Deps minimal halten; neue Deps nur mit Messbeleg.
Bei Neueinführung: neueste Version nehmen (auch bei
Kompatibilitätswarnung), Usage-Example vorab in Plan + hier nachtragen.
deepwiki-MCP war im Container bisher ohne Transport; die
deepwiki-Spalte hält Abfrageschlüssel für später vor. Lokaler Ersatz:
`SUPPORTED_FORMS.md`, `cargo info`, Crate-Docs.

## Dependencies (Planungsstand, Newest-Check bei Schritt 0 zu bestätigen)

| Crate / Tool | Stand (20.09., zu bestätigen) | Verwendet in (iter2) | GitHub-Org/Repo | deepwiki-Vorschlag |
|---|---|---|---|---|
| `macroquad 0.4` | Lock-Stand Iter1: `0.4.16`; `cargo search` heute: `0.4.16` = newest | Rust seriell + parallel (Fenster/Input) | `not-fl3/macroquad` | `not-fl3/macroquad` |
| `miniquad 0.4` (transitiv) | transitiv | Rust GUI (transitiv) | `not-fl3/miniquad` | `not-fl3/miniquad` |
| `sbcl` 2.6.0.debian | System | `lisp/gen.lisp`-Lauf | — | — |
| `cl-cpp-generator2` (Quicklisp-Local) | Tooling | C++-Generierung (nur iter2-Core) | `plops/cl-cpp-generator2` | `plops/cl-cpp-generator2` |
| PGE3 (`olcPixelGameEngine3.h`, main) | vendorn + Version pinnen | `treemap_gui` (Draw/String/Input) | `OneLoneCoder/olcPixelGameEngine3` | `OneLoneCoder/olcPixelGameEngine3` |

**Keine neue Cargo-Dep geplant.** Kandidat nur mit Messbeleg
(z.B. `rayon`-Mess-Arm — Übernahme nur bei Speedup-Beleg).

## Neueste-Version-Check (bei Schritt 0 ausführen und hier nachtragen)

- `cargo search macroquad` → ? (Erwartung: `0.4.16` = Lock-Stand → kein Update).
- Kandidaten bei Bedarf: `cargo search <kandidat>` + `cargo info <kandidat>`.
- Regel: bei Neueinführung neueste Version (`cargo add <crate>@latest`
  sinngemäß), Kompatibilitätswarnung dokumentieren, nicht umgehen.

## Usage-Examples (vorab, aus Iter1 übernommen)

- GUI ohne Fenster-Flag für CLI-Modi (Muster `01_more`):
  `macroquad::Window::from_config(conf, gui_main())` statt
  `#[macroquad::main]` — hält `--scan`/`--bench` fensterlos.
- Parallel-Scan ohne neue Dep:
  `std::thread::scope(|s| { s.spawn(|| subtree); })` über Owned Values;
  Dateien inline, Threads nur pro Subdir, Cap 4×nCPU, Re-Sort nach Join.
- Transpiler-Lauf: `sbcl --load iter2/lisp/gen.lisp --quit` (Exit 0);
  Paren-Gate: `parenmedic diagnose iter2/lisp/` grün.
- C++-Minimalbeispiel (Vorbild):
  `plan/20260919_01_merge/gen-cpp-freestanding-example.lisp`
  (`write-source`, `lprint`-`defun` mit `(:as "label" expr)`).
- C++-Bench-Preset: sanitize-freies Release-Binary messen;
  Sanitizer-Presets (ASan/UBSan, empfohlen +TSan) nur für Tests.

## Systemseitig (Docker, Ubuntu 26)

Aus Iter1 belegt, für iter2 einplanen: Laufzeit
`xvfb libxkbcommon0 libxi6 libx11-6 libgl1`; Build
`libx11-dev libxi-dev libgl1-mesa-dev libasound2-dev cmake g++ sbcl`;
GUI-Smoke `openbox` (gegen PGE-BadAtom) + `x11-apps` (`xwd` für
Screenshot-Beweis); Linter `clang-tidy clang-format`; Paren-Tool
`parenmedic` (lokal gebaut). Neue Programme am Ende in `walkthrough.md`
als `apt-get`-Zeile festhalten.
