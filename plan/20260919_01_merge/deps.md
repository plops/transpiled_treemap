# deps.md — Abhängigkeiten (Stand 2026-09-19, verifiziert)

Vorgabe: Code und Deps minimal halten; neue Deps nur mit Messbeleg.
deepwiki-MCP war in diesem Container ohne Transport nicht verfügbar;
die deepwiki-Spalte hält Abfrageschlüssel für später vor.

## Dependencies

| Crate / Tool | Stand | Verwendet in | GitHub-Org/Repo | deepwiki-Vorschlag |
|---|---|---|---|---|
| `macroquad 0.4` (Lock 0.4.16) | newest = Lock | Rust GUI + Tests | `not-fl3/macroquad` | `not-fl3/macroquad` |
| `miniquad 0.4` (transitiv) | transitiv | Rust GUI (transitiv) | `not-fl3/miniquad` | `not-fl3/miniquad` |
| `sbcl` 2.6.0.debian | System | `cpp/gen.lisp`-Lauf | — | — |
| `cl-cpp-generator2` (Quicklisp-Local) | Tooling | C++-Generierung | `plops/cl-cpp-generator2` | `plops/cl-cpp-generator2` |
| `cl-ppcre` (Quicklisp) | Tooling | Beispiel-Muster 09 | — | — |
| PGE3 (`olcPixelGameEngine3.h`, 790.014 Bytes, main) | vendored unter `cpp/third_party/` | `treemap_gui` (Draw/String/Input) | `OneLoneCoder/olcPixelGameEngine3` | `OneLoneCoder/olcPixelGameEngine3` |

**Keine neue Cargo-Dep** eingeführt (`rayon` evaluiert, verworfen s.u.).
Keine Dev-Deps (`criterion`/`tempfile`/`assert_cmd`): Bench via
`std::time::Instant` + `eprintln!`, Black-Box-Tests via
`std::process::Command`. C++ ohne jede Third-Party-Dep (nur STL +
PGE3-Header für die optionale GUI-Schale).

## Neueste-Version-Check

- `cargo search macroquad` → `0.4.16` = Lock-Stand → kein Update.
- `cargo search rayon` → `1.12.0`: **nicht übernommen**. Begründung:
  `std::thread::scope` + Owned-Subtrees erreicht 4–5x Scan-Speedup ohne
  neue Dep; `rayon` brächte hier keinen belegten Mehrwert (I/O-bound,
  Granularität ist der Hebel, nicht der Scheduler).

## Usage-Examples (bei Einführung/Wechsel nötig)

- Keine neue Dep → keine neuen Usage-Examples. Referenzierte Muster:
  `macroquad::Window::from_config(conf, gui_main())` (aus
  `01_more/src/main.rs`), `thread::scope`/`std::thread`-Fanout (eigene
  Implementierung, s. Walkthrough).

## Systemseitig (Docker)

Neu installiert: `xvfb`, `libx11-dev`, `libxi-dev`, `libgl1-mesa-dev`,
`libasound2-dev`, `libxkbcommon0` (Laufzeit für miniquad), `libpng-dev`
(nur Link-Dep des PGE-Headers), `openbox` + `x11-apps` (PGE-Smoke: WM
gegen BadAtom, `xwd`-Screenshot).
Aufzunehmen in Container-Images:
`apt-get install -y xvfb libxkbcommon0 libxi6 libx11-6 libgl1`
(Laufzeit) bzw. `-dev`-Pendants für Builds.
