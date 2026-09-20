# Walkthrough: Technische Dokumentation (`plan/20260920_02_doc`)

Datum: 2026-09-20 · Stand: `doc.md` + `prompt.txt` geschrieben und committet,
diese Datei schließt die Arbeit ab.

## Experimente

1. **Repo-Kartierung:** `iter1/src/main.rs` (672 Zeilen) vs.
   `iter2/src/main.rs` (662 Zeilen) per `diff` verglichen — algorithmisch
   identisch, iter2 faktorisiert `is_virtual`/`dir_node`/`file_node` heraus
   und trägt Cap/`depth` von Anfang an. C++-Diff (`010_scanner.hpp`):
   Pfade by value, `ScanWorkerLimit` ohne Underflow-Falle, gleiche
   Fan-out-/Join-Disziplin. Ergebnis in `doc.md` § 2.2 tabelliert.
2. **Eigene Benchmark-Stichprobe** (kein Workflow — Mehr-Agenten-Overhead
   für diese Dateimenge nicht gerechtfertigt): je 5 Runden `--bench` mit den
   vorhandenen Release-Binaries, ohne Neu-Build:
   - `/usr` (10,5 GB): Rust 1,00–1,12×, C++ direkt 1,47–1,59×.
   - `/root` (96,9 GB): Rust 1,12–1,57×, C++ direkt 1,79–2,49×.
   - Beide Messungen decken `report_bench_workspace.md` (Rust 1,00–1,74× /
     C++ 1,53–2,59× je nach Baum) — die Reports sind reproduzierbar, die
     initialen Fixture-Annahmen (4–8×) als warmer-Cache-Artefakt eingeordnet.
3. **Antwort auf die Leitfragen** (`doc.md` § 5.2–5.3): Annahmen bestätigt
   mit Korrektur (Speedup schrumpft mit Baumgröße/Cache-Lage; C++ seriell
   ~1,5× hinter Rust, parallel vorn; Layout irrelevant). Architektur für
   große Dateisysteme ausreichend (97 GB in ~1–1,5 s, GUI lädt im
   Hintergrund); Überarbeitungs-Kriterien dokumentiert (Work-Stealing erst
   bei Millionen Dateien/Netz-FS, `inotify`-Schicht für Live-Updates,
   `rayon` weiter ohne Beleg).
4. **Mermaid-Härtung:** Labels mit `<`/`>` in zwei Diagrammen gequotet, damit
   das GitHub-Rendering nicht bricht.

## Learnings

- Die teuersten Architektur-Fakten des Repos stecken in je einer Zeile
  (Single-Stat, Thread-pro-Datei, Cap, Top-Level-only) — Doku mit Formeln
  (`Scan-Zeit`, Amdahl, worst-Ratio) macht sie nachrechenbar statt anekdotisch.
- `diff` zwischen den Iterationen ist ergiebiger als Neu-Lesen: Der gesamte
  iter1→iter2-Unterschied passt in eine Tabelle (§ 2.2).
- Ein Commit dieser Session zog eine fremde, bereits gestagte Umbenennung
  (`plan/20260920_02_redo` → `plan/20260920_01_redo`) mit ein — vor dem
  Commit `git status` prüfen, auch wenn man nur eigene Dateien addet.

## Quellen

- Code: `iter1/src/main.rs`, `iter2/src/main.rs`, `iter1/cpp/src/`,
  `iter2/cpp/src/010_scanner.hpp`, `iter2/cpp/src/015_color.hpp`,
  `iter2/cpp/src/020_layout.hpp`, `iter2/lisp/gen.lisp`,
  `iter2/scripts/setup03_rust_bench.sh`.
- Pläne/Reports: `plan/20260919_01_merge/walkthrough.md`,
  `plan/20260919_01_merge/rust_cpp_vergleich.md`,
  `plan/20260920_01_redo/walkthrough.md`,
  `plan/20260920_01_redo/report_rust_cpp.md`,
  `plan/20260920_01_redo/report_bench_workspace.md`.
- Algorithmus: Bruls / Huizing / van Wijk, „Squarified Treemaps"
  (worst-Seitenverhältnis, Zeilenpass).
- GitHub-Rendering: MathJax (`$…$`, `$$…$$`) und Mermaid
  (```` ```mermaid ````) laut Aufgabenstellung nativ unterstützt.
