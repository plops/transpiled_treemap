# Benchmark-Report: reale Bäume (iter2, 2026-09-20)

Ziel: `setup03` (Rust), `setup05` (C++ direkt) und `setup09` (C++ generiert)
auf realen Bäumen vergleichen. Jeweils 5 Runden seriell vs. parallel, inkl.
Layout-Zeit.

## Methode

```sh
./iter2/scripts/setup03_rust_bench.sh /workspace   # 48,8 GB
./iter2/scripts/setup05_cpp_test_bench.sh /workspace
./iter2/scripts/setup09_transpiled_bench.sh /workspace
# ebenso mit /root (96,9 GB) und /usr (10,5 GB)
```

Binaries: `iter2/target/release/treemap_iter2`,
`iter2/cpp/build/bench/treemap_cpp`,
`iter2/cpp/build/gen-bench/treemap_cpp`.
`setup05` enthält zusätzlich `ctest` (Sanitizer-Build) und
`--scan`-Parität Rust==C++.

Hinweis: ein Lauf auf `/` scheiterte bei allen drei Binaries mit
`serial vs parallel size mismatch` (Differenz wenige hundert Byte bei
~170 GB) — erwartbar auf live `/proc|/sys|/tmp`. `/workspace`, `/root`
und `/usr` waren über alle Runden stabil (je 5 Runden × 3 Binaries).

## Ergebnisse: /workspace (48,8 GB)

### Rust (`setup03`), Speedup 1,22–1,41x

| Runde | seriell Scan | seriell Layout | parallel Scan | parallel Layout | Speedup |
|---|---|---|---|---|---|
| 1 | 0,528 s | 0,009 s | 0,373 s | 0,007 s | 1,41x |
| 2 | 0,442 s | 0,008 s | 0,338 s | 0,007 s | 1,31x |
| 3 | 0,444 s | 0,008 s | 0,360 s | 0,006 s | 1,23x |
| 4 | 0,442 s | 0,008 s | 0,364 s | 0,006 s | 1,22x |
| 5 | 0,445 s | 0,008 s | 0,335 s | 0,007 s | 1,33x |

### C++ direkt (`setup05`), Speedup 2,07–2,43x

`ctest`: 1/1 bestanden. `scan parity rust==cpp: OK`.

| Runde | seriell Scan | seriell Layout | parallel Scan | parallel Layout | Speedup |
|---|---|---|---|---|---|
| 1 | 0,744 s | 0,0068 s | 0,349 s | 0,0069 s | 2,14x |
| 2 | 0,681 s | 0,0069 s | 0,327 s | 0,0062 s | 2,08x |
| 3 | 0,680 s | 0,0067 s | 0,280 s | 0,0064 s | 2,43x |
| 4 | 0,679 s | 0,0069 s | 0,329 s | 0,0068 s | 2,07x |
| 5 | 0,677 s | 0,0069 s | 0,311 s | 0,0063 s | 2,18x |

### C++ generiert (`setup09`), Speedup 2,07–2,58x

| Runde | seriell Scan | seriell Layout | parallel Scan | parallel Layout | Speedup |
|---|---|---|---|---|---|
| 1 | 0,810 s | 0,0070 s | 0,314 s | 0,0068 s | 2,58x |
| 2 | 0,679 s | 0,0069 s | 0,328 s | 0,0072 s | 2,07x |
| 3 | 0,677 s | 0,0069 s | 0,314 s | 0,0072 s | 2,15x |
| 4 | 0,677 s | 0,0067 s | 0,312 s | 0,0071 s | 2,17x |
| 5 | 0,676 s | 0,0070 s | 0,276 s | 0,0075 s | 2,45x |

## Ergebnisse: /root (96,9 GB)

`ctest` in `setup05`: 1/1 bestanden. `scan parity rust==cpp: OK`.

### Rust (`setup03`), Speedup 1,15–1,74x (parallel streut)

| Runde | seriell Scan | seriell Layout | parallel Scan | parallel Layout | Speedup |
|---|---|---|---|---|---|
| 1 | 1,842 s | 0,028 s | 1,061 s | 0,015 s | 1,74x |
| 2 | 1,698 s | 0,029 s | 1,103 s | 0,014 s | 1,54x |
| 3 | 1,686 s | 0,029 s | 1,462 s | 0,014 s | 1,15x |
| 4 | 1,686 s | 0,031 s | 0,999 s | 0,015 s | 1,69x |
| 5 | 1,683 s | 0,029 s | 1,416 s | 0,014 s | 1,19x |

### C++ direkt (`setup05`), Speedup 1,91–2,14x

| Runde | seriell Scan | seriell Layout | parallel Scan | parallel Layout | Speedup |
|---|---|---|---|---|---|
| 1 | 2,740 s | 0,0255 s | 1,361 s | 0,0147 s | 2,01x |
| 2 | 2,582 s | 0,0253 s | 1,301 s | 0,0157 s | 1,98x |
| 3 | 2,588 s | 0,0256 s | 1,356 s | 0,0142 s | 1,91x |
| 4 | 2,582 s | 0,0250 s | 1,209 s | 0,0150 s | 2,14x |
| 5 | 2,582 s | 0,0253 s | 1,353 s | 0,0150 s | 1,91x |

### C++ generiert (`setup09`), Speedup 1,73–2,59x (parallel streut)

| Runde | seriell Scan | seriell Layout | parallel Scan | parallel Layout | Speedup |
|---|---|---|---|---|---|
| 1 | 2,831 s | 0,0251 s | 1,474 s | 0,0155 s | 1,92x |
| 2 | 2,591 s | 0,0256 s | 1,258 s | 0,0142 s | 2,06x |
| 3 | 2,604 s | 0,0254 s | 1,231 s | 0,0144 s | 2,12x |
| 4 | 2,611 s | 0,0255 s | 1,506 s | 0,0155 s | 1,73x |
| 5 | 2,581 s | 0,0256 s | 0,997 s | 0,0152 s | 2,59x |

## Ergebnisse: /usr (10,5 GB)

`ctest` in `setup05`: 1/1 bestanden. `scan parity rust==cpp: OK`.

### Rust (`setup03`), Speedup 1,00–1,06x (kein Gewinn)

| Runde | seriell Scan | seriell Layout | parallel Scan | parallel Layout | Speedup |
|---|---|---|---|---|---|
| 1 | 0,226 s | 0,003 s | 0,214 s | 0,002 s | 1,06x |
| 2 | 0,208 s | 0,003 s | 0,200 s | 0,002 s | 1,04x |
| 3 | 0,199 s | 0,003 s | 0,198 s | 0,003 s | 1,00x |
| 4 | 0,198 s | 0,003 s | 0,196 s | 0,003 s | 1,01x |
| 5 | 0,199 s | 0,003 s | 0,197 s | 0,002 s | 1,01x |

### C++ direkt (`setup05`), Speedup 1,53–1,84x

| Runde | seriell Scan | seriell Layout | parallel Scan | parallel Layout | Speedup |
|---|---|---|---|---|---|
| 1 | 0,316 s | 0,0031 s | 0,171 s | 0,0026 s | 1,84x |
| 2 | 0,263 s | 0,0030 s | 0,172 s | 0,0023 s | 1,53x |
| 3 | 0,263 s | 0,0030 s | 0,172 s | 0,0023 s | 1,53x |
| 4 | 0,261 s | 0,0030 s | 0,169 s | 0,0026 s | 1,54x |
| 5 | 0,262 s | 0,0030 s | 0,172 s | 0,0023 s | 1,53x |

### C++ generiert (`setup09`), Speedup 1,51–1,64x

| Runde | seriell Scan | seriell Layout | parallel Scan | parallel Layout | Speedup |
|---|---|---|---|---|---|
| 1 | 0,314 s | 0,0036 s | 0,191 s | 0,0025 s | 1,64x |
| 2 | 0,265 s | 0,0030 s | 0,172 s | 0,0023 s | 1,54x |
| 3 | 0,283 s | 0,0031 s | 0,172 s | 0,0026 s | 1,64x |
| 4 | 0,264 s | 0,0030 s | 0,168 s | 0,0025 s | 1,57x |
| 5 | 0,261 s | 0,0030 s | 0,173 s | 0,0024 s | 1,51x |

## Lesart

* Speedup klein, weil der Scan Syscall-/VFS-gebunden ist (`readdir` +
  `file_type`/`metadata`), nicht CPU-gebunden. Die Verzeichnisliste je
  Ebene bleibt seriell; nur Subdirs fächern auf (Cap 4×nCPU). Dazu
  Thread-Spawn-Kosten pro Subdir.
* Über alle drei Bäume dasselbe Bild: Rust seriell schneller
  (/workspace ~0,44 s vs. ~0,68 s; /root ~1,69 s vs. ~2,58 s;
  /usr ~0,20 s vs. ~0,26 s), parallel leicht hinten oder gleichauf —
  daher der kleinere Rust-Speedup. Wahrscheinliche Gründe:
  `metadata()`-Stat pro Datei plus `thread::scope`-Overhead gegenüber
  `d_type`-Pfad und rohen `std::thread`s in C++.
* Kleiner Baum, kleiner Gewinn: auf `/usr` (10,5 GB, ~0,2 s) frisst der
  Thread-Overhead den Rust-Gewinn komplett auf (1,00–1,06x); C++ hält
  ~1,5x. Auf `/root` streut die parallele Zeit bei Rust und Generat
  deutlich (I/O-Konkurrenz bei ~1–1,5 s Scan).
* Layout irrelevant für den Speedup (ms vs. Scan in Zehntelsekunden
  bis Sekunden).
* Generat ≈ direkt (gleiche Architektur).
