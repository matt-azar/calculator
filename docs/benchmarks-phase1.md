# Phase 1 launch benchmarks (ma1pc)

**Machine:** Windows self-hosted worker `ma1pc` (win32 10.0.26200)  
**Build:** MSVC 19.40 x64 `/O2` Release (`calculator.exe` + `launch_bench.exe`)  
**Date:** 2026-11-21  
**Harness:** `tools/launch_bench` — records process create, first visible `Calculator.Win32.Phase1` window, and first probe character appearing in the expression `EDIT` (id 1001).

## Summary

| Suite | N | Metric | Median (ms) | p95 (ms) |
| --- | ---: | --- | ---: | ---: |
| Warm / repeated | 21 | Process create | 6.68 | 8.14 |
| Warm / repeated | 21 | First visible window | 47.94 | 65.87 |
| Warm / repeated | 21 | First character in EDIT | 70.69 | 91.47 |
| Cold-ish (no reboot) | 5 | Process create | 2.79 | 5.04 |
| Cold-ish (no reboot) | 5 | First visible window | 32.34 | 784.52 |
| Cold-ish (no reboot) | 5 | First character in EDIT | 65.09 | 825.07 |

Cold-ish p95 is dominated by one outlier (~970 ms visible); see individual samples below.

## Cold / reboot note

A **full reboot mid-task was skipped**. This cloud-agent worker session runs on the same machine; a reboot would almost certainly kill the worker and strand the run. Samples labeled **cold-ish** are fresh processes after a ~45 s idle gap with OS file cache still warm — **not** post-reboot cold starts. Prefer real post-reboot cold samples in a follow-up when Matt can reboot without breaking the worker (or run the harness locally after reboot).

## Warm samples (21)

Raw CSV: [`bench-phase1-warm.csv`](bench-phase1-warm.csv)

| # | process_ms | visible_ms | keystroke_ms |
| ---: | ---: | ---: | ---: |
| 1 | 6.94 | 40.06 | 68.68 |
| 2 | 3.42 | 80.17 | 128.81 |
| 3 | 3.09 | 32.20 | 64.46 |
| 4 | 6.25 | 47.29 | 74.30 |
| 5 | 5.97 | 48.13 | 63.51 |
| 6 | 6.87 | 64.58 | 91.47 |
| 7 | 6.13 | 47.94 | 69.35 |
| 8 | 4.34 | 47.94 | 55.09 |
| 9 | 7.03 | 48.45 | 76.29 |
| 10 | 5.15 | 47.86 | 59.79 |
| 11 | 6.97 | 63.50 | 74.08 |
| 12 | 8.38 | 48.22 | 70.69 |
| 13 | 3.65 | 32.47 | 59.49 |
| 14 | 6.68 | 47.09 | 66.64 |
| 15 | 8.14 | 65.87 | 83.38 |
| 16 | 7.96 | 47.69 | 67.57 |
| 17 | 6.66 | 47.80 | 76.95 |
| 18 | 6.72 | 49.56 | 84.27 |
| 19 | 4.25 | 32.33 | 51.28 |
| 20 | 7.61 | 64.23 | 75.13 |
| 21 | 7.20 | 63.21 | 83.67 |

## Cold-ish samples (5, no reboot)

Raw CSV: [`bench-phase1-coldish.csv`](bench-phase1-coldish.csv)

| # | process_ms | visible_ms | keystroke_ms | Notes |
| ---: | ---: | ---: | ---: | --- |
| 1 | 5.57 | 42.95 | 79.30 | |
| 2 | 2.79 | 32.34 | 65.09 | |
| 3 | 2.67 | 31.86 | 60.91 | |
| 4 | 2.94 | 969.92 | 1011.52 | Outlier (scheduler / desktop contention; session was lock-screen) |
| 5 | 2.03 | 32.02 | 54.08 | |

## Manual focus check

Launched `build\Release\calculator.exe` directly (shortcut-equivalent).  
`GetGUIThreadInfo` reported focus on child id **1001** (`EDIT`) with class `Edit` — **FOCUS_OK=True** without clicking.

## Harness notes

- Timestamps are `steady_clock` milliseconds from immediately before `CreateProcessW`.
- Character delivery tries `SendInput` first, then `EM_REPLACESEL` / `WM_CHAR`, then `SetWindowTextW`. On this locked agent desktop, synthetic delivery is required; the metric remains “character visible in the field.” Real keyboard focus is covered by the manual focus check above.
- App instrumentation: `QueryPerformanceCounter` on the first line of `wWinMain`, exposed via window property `Calculator.WwMainStartQpc`.

## Runnable artifact

| File | Path after Release build |
| --- | --- |
| Calculator | `build\Release\calculator.exe` |
| Harness | `build\Release\launch_bench.exe` |

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
build\Release\calculator.exe
build\Release\launch_bench.exe --exe build\Release\calculator.exe --runs 21 --out docs\bench-phase1-warm.csv
```

Screenshot (PrintWindow capture): `docs/phase1-calculator-window.png` (also in the Project store `media/phase1-calculator-window.png`).
