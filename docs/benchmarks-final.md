# Final launch benchmarks (ma1pc)

See also the Project store copy used for coordination. Authoritative Phase 3 numbers from this machine:

## Minimal (Phase 1) vs finished (Phase 3)

| Build | Suite | N | Metric | Median (ms) | p95 (ms) |
| --- | --- | ---: | --- | ---: | ---: |
| Minimal (P1) | Warm | 21 | First char in EDIT | 70.69 | 91.47 |
| Finished (P3) | Warm | 21 | Process create | 3.62 | 8.21 |
| Finished (P3) | Warm | 21 | First visible | 32.53 | 64.07 |
| Finished (P3) | Warm | 21 | First char in EDIT | **57.37** | 97.49 |
| Finished (P3) | Cold-ish | 5 | First char in EDIT | 57.33 | 814.52 |
| Finished (P3) | Warm | 21 | Launch → `1+1` result | **63.39** | 81.88 |

Finished first-char is not slower than Phase 1 — **no WPR/WPA**. Cold-ish = no reboot (worker session). Tauri compare skipped (no Release install; debug-only tree elsewhere).

Raw CSVs: `bench-phase3-warm.csv`, `bench-phase3-coldish.csv`, `bench-phase3-first-result.csv`.

Functional: type/paste, evaluate, resize, close/reopen, focus — **PASS**. `eval_tests` — **PASS**.
