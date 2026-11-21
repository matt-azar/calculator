# Phase 2 launch check (ma1pc)

**Machine:** Windows self-hosted worker `ma1pc`  
**Build:** MSVC x64 `/O2` Release with in-process evaluator linked (session constructed on first Enter only)  
**Date:** 2026-11-21  
**Branch:** `cursor/phase2-eval-f834` (stacked on Phase 1)

## Launch path

`wWinMain` / `WM_CREATE` still do **not** construct `calc::eval::Session`. The unique_ptr stays null until the EDIT subclass handles the first `VK_RETURN`.

## Warm launch_bench vs Phase 1 baseline

| Metric | Phase 1 median / p95 (ms) | Phase 2 median / p95 (ms) | Delta median |
| --- | --- | --- | --- |
| Process create | 6.68 / 8.14 | 3.33 / 8.56 | −3.35 |
| First visible | 47.94 / 65.87 | 33.02 / 80.78 | −14.92 |
| First char in EDIT | 70.69 / 91.47 | 68.36 / 112.03 | −2.33 |

N = 21 both runs. Phase 2 p95 includes a ~1000 ms visible outlier (sample 4); median first-char is flat-to-slightly-faster vs Phase 1 — **no meaningful launch regression** from linking the evaluator.

Raw CSV: [`bench-phase2-warm.csv`](bench-phase2-warm.csv)

## Unit tests

`build\Release\eval_tests.exe` — all passed, including Python `**` cases and smoke (`sin(pi/2)`, `mean([1,2,3,4])`, `x=2**10` then `x+1`, `_`, history).
