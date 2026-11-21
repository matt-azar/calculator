# Calculator

Native **C++ / Win32** x64 calculator for Windows with an **in-process** math evaluator (no Python). Primary metric: **cold launch → first keystroke visibly appearing in the input field**.

## Requirements

- Windows x64
- Visual Studio 2022 with **Desktop development with C++** (MSVC)
- CMake 3.20+ (VS-bundled CMake is fine)

## Build (MSVC x64 Release)

If `cmake -G "Visual Studio 17 2022"` fails and only lists Unix Makefiles / Ninja, your PATH is picking **MSYS/MinGW CMake** (`C:\msys64\usr\bin\cmake.exe`). Use Visual Studio’s CMake instead (or open a **x64 Native Tools** / VS Developer prompt):

```bat
set CMAKE="C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
%CMAKE% -S . -B build -G "Visual Studio 17 2022" -A x64
%CMAKE% --build build --config Release
```

Or, with VS CMake already first on `PATH`:

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Helper: `build.cmd` (finds VS CMake and builds Release).

| Target | Path |
| --- | --- |
| Calculator | `build\Release\calculator.exe` |
| Launch harness | `build\Release\launch_bench.exe` |
| Eval unit tests | `build\Release\eval_tests.exe` |

```bat
build\Release\eval_tests.exe
```

## Run

```bat
build\Release\calculator.exe
```

- Standard Win32 frame; focus on the expression field on activate
- Typing / editing / paste work immediately
- **Evaluator/session created on first Enter only** (not on launch)
- Up / Down recalls in-memory history after the first evaluation

Examples:

```text
sin(pi/2)
mean([1, 2, 3, 4])
x = 2**10
x + 1
_
```

Python-style `**`: `-2**2` → `-4`, `2**-3` → `0.125`, `2**3**2` → `512`.

## Launch benchmarks

```bat
build\Release\launch_bench.exe --exe build\Release\calculator.exe --runs 21
build\Release\launch_bench.exe --exe build\Release\calculator.exe --mode first-result --runs 21
```

`--mode first-result` times launch → evaluated `1+1` (via `WM_COPYDATA` + `last_result.txt` side channel).

Final numbers: [`docs/benchmarks-final.md`](docs/benchmarks-final.md). Phase 1/2 detail: `docs/benchmarks-phase1.md`, `docs/benchmarks-phase2.md`.

## Layout

```
CMakeLists.txt
src/main.cpp              # wWinMain (QPC first line; no eval init)
src/ui/window.hpp|cpp     # frame, EDIT, lazy Session, WM_COPYDATA eval
src/eval/                 # tokenizer, parser, builtins, session (Win32-free)
tools/launch_bench/       # keystroke + first-result harness
tests/eval_tests.cpp
docs/
```

## Status

| Phase | Status |
| --- | --- |
| 1 — Minimal window + harness | Done (PR #3) |
| 2 — In-process evaluator | Done (PR #4) |
| 3 — Finished-build verification | This branch |

Earlier Tauri + IPython sources remain for history but are **superseded**.
