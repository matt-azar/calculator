# Calculator

Native **C++ / Win32** x64 calculator for Windows. In-process math evaluator (no Python). Primary metric: **cold launch → first keystroke visibly appearing in the input field**.

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
| Eval unit tests | `build\Release\eval_tests.exe` |

```bat
build\Release\eval_tests.exe
```

## Run

```bat
build\Release\calculator.exe
```

- Standard Win32 frame; focus goes to the expression `EDIT` on activate
- Typing / editing / paste work immediately
- **Evaluator and session are created on first Enter only** (not on the launch path)
- Up / Down recalls in-memory input history after the first evaluation

Try:

```text
sin(pi/2)
mean([1, 2, 3, 4])
x = 2**10
x + 1
_
```

Python-style `**`: `-2**2` → `-4`, `2**-3` → `0.125`, `2**3**2` → `512`.

## Launch benchmark

```bat
build\Release\launch_bench.exe --exe build\Release\calculator.exe --runs 21 --out docs\bench-phase2-warm.csv
```

See `docs/benchmarks-phase1.md` and `docs/benchmarks-phase2.md`.

## Layout

```
CMakeLists.txt
src/main.cpp                 # wWinMain (QPC on first line; no eval init)
src/ui/window.hpp|cpp        # frame, EDIT, lazy Session on Enter
src/eval/                    # tokenizer, parser, builtins, session (Win32-free)
tools/launch_bench/          # launch → visible → keystroke harness
tests/eval_tests.cpp
docs/
```

## Status

| Phase | Status |
| --- | --- |
| 1 — Minimal window + harness | Done (PR #3) |
| 2 — In-process math evaluator | This branch |
| 3 — Finished-build verification | Not started |

The earlier Tauri + IPython prototype remains in-tree for history but is **superseded**.
