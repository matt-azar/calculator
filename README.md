# Calculator

Native **C++ / Win32** x64 calculator for Windows. Phase 1 ships a minimal focused `EDIT` window and an external launch-benchmark harness. The math evaluator arrives in Phase 2 (in-process C++; no Python runtime).

Primary metric: **cold launch → first keystroke visibly appearing in the input field**.

## Requirements

- Windows x64
- Visual Studio 2022 with **Desktop development with C++** (MSVC)
- CMake 3.20+ (the CMake bundled with VS 2022 is fine)

## Build (MSVC x64 Release)

From a **x64 Native Tools** / Developer PowerShell for VS 2022, or after `vcvars64.bat`:

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Artifacts:

| Target | Path |
| --- | --- |
| Calculator | `build\Release\calculator.exe` |
| Launch harness | `build\Release\launch_bench.exe` |

## Run

```bat
build\Release\calculator.exe
```

The window uses a standard Win32 frame. On activate, focus goes to the single-line expression field so typing, editing, and paste work immediately. Phase 1 does **not** initialize an evaluator, prefs, or history on the launch path.

## Launch benchmark

```bat
build\Release\launch_bench.exe --exe build\Release\calculator.exe --runs 21 --out docs\bench-phase1-warm.csv
```

The harness records, per launch:

1. Process create (`CreateProcessW` return)
2. First visible main window (`Calculator.Win32.Phase1`)
3. First keyboard event whose character appears in the `EDIT` field

It prints **median** and **p95** for successful samples. See `docs/benchmarks-phase1.md` for numbers from ma1pc.

## Layout (Phase 1)

```
CMakeLists.txt
src/main.cpp              # wWinMain (QPC timestamp on first line)
src/ui/window.hpp|cpp     # frame, EDIT, result area, focus-on-activate
tools/launch_bench/       # external launch → visible → keystroke harness
docs/benchmarks-phase1.md
```

## Status

| Phase | Status |
| --- | --- |
| 1 — Minimal window + harness | In progress (this tree) |
| 2 — In-process math evaluator | Not started |
| 3 — Finished-build verification | Not started |

The earlier Tauri + IPython prototype remains in-tree for history but is **superseded** by this native Win32 direction.
