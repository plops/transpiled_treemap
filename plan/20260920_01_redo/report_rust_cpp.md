# Report: Rust vs. C++ (iter2, Stand 2026-09-20)

## Messungen

Fixture: `/tmp/treemap_iter2_bench` (deterministisch, 40×100×1 KiB = 4000
Dateien, 3,9 MB). Großer Baum: `/workspace/src` (47,8 GB).

| Implementierung | Fixture seriell | Fixture parallel | Speedup | Groß seriell | Groß parallel | Speedup |
|---|---|---|---|---|---|---|
| Rust (`--release`) | ~6–9 ms | ~1–2 ms | ~4–7x | ~0,34 s | ~0,29–0,32 s | ~1,1–1,3x |
| C++ direkt (Release) | ~10–12 ms | ~1,3–1,9 ms | ~6–7,5x | ~0,53 s | ~0,22–0,30 s | ~1,9–2,4x |
| C++ generiert (`-O2`) | ~10–13 ms | ~1,4–2,1 ms | ~6,3–7,7x | ~0,53 s | ~0,20–0,30 s | ~1,9–2,7x |

Layout-Zeiten: Fixture < 1 ms (kein Speed-Claim, nur Parität); groß:
Rust 0,006 → 0,003 s (2x), C++ 0,0057 → 0,0028 s (2x), generiert gleich.
Größen überall identisch (47,8 GB / 3,9 MB); `--scan` byte-identisch
zwischen allen drei Binaries; Missing-Dir überall `0.0 B in 0 files`,
Exit 0.

Lesart: C++ seriell bleibt ~1,5x hinter Rust (`std::filesystem`-Overhead);
auf dem parallelen Pfad (den die GUI nutzt) Parität bzw. C++ vorn.
Generat ≈ direkt (gleiche Architektur, gleiche Disziplin).

## Variablen-Parität und Safety-Fazit

Frage aus dem Auftrag: Geht C++ mit Variablen genauso um wie Rust? Erbt
C++ seine Sicherheit von Rust? Antwort: **Nein — kein Erbe.** Gleiche
Struktur reduziert nur die Review-Fläche. Befund je Klasse:

| Klasse | Rust | C++-Disziplin (iter2) | Prüfung | Befund |
|---|---|---|---|---|
| Thread-Übergabe | Owned via Scope-Typ | `std::move`, disjunkte `results[i]`-Slots | ASan/UBSan + Tests | grün |
| Join | automatisch | Join-Schleife im selben Scope, keine frühen Returns | Review | grün |
| Use-after-move | verboten | Review-Verbot | tidy | grün |
| Pfad-Lifetimes | `PathBuf`-Ownership | **iter2-Fix: Pfade by value**, kein `const&` an Iterator-Interna | `bugprone-dangling-handle` in Config | grün |
| Fehlerpfade | `Result` zwingt | jede `error_code`-Stelle geprüft + `report_skip` | Review + Missing-Dir-Test | grün |
| Sortierung | Typ-Disjunktheit | `sort` nach Join | Sortier-Invarianten-Test | grün |
| Defaults | Member-Init | **Generat-Lücke gefunden**: `defstruct0` ignoriert Init-Forms → `isDir` explizit `true` gesetzt (Missing-Dir-Divergenz `1 files` → `0 files`) | Byte-Paritätstest | grün nach Fix |

Sanitizer-Builds sind 3–5x langsamer — Bench-Zahlen gelten für
sanitize-freie Builds; Tests laufen immer mit Sanitizern.
