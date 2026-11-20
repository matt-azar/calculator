# Calculator

Windows taskbar calculator that opens a **resizable IPython console** with common math helpers already imported. The window frame and theme pair with [matt-azar/text-editor](https://origin.cursor.com/matt-azar/text-editor): frameless Tauri 2 chrome, 36px custom titlebar, shared CSS tokens, Segoe UI Variable Text.

Try:

```python
sin(pi/2)
mean([1, 2, 3, 4])
_
```

## Requirements

- Node.js 18+ and npm
- Rust (stable) with Tauri 2 prerequisites
- Python 3 with IPython: `pip install ipython`
- Windows recommended for the full desktop experience (NSIS packaging is Windows-first). Linux/macOS can run `tauri dev` for the shell + PTY prototype if WebKit/GTK deps are installed.

## Develop

```bash
npm install
npm run tauri -- dev
```

This starts Vite on port 1420 and opens the frameless Calculator window with a live IPython session.

Frontend-only (no PTY):

```bash
npm run dev
```

## Build (Windows installer)

```bash
npm run tauri -- build -- --bundles nsis
```

The installer product name is **Calculator** (`com.calculator.app`) so it does not collide with the text-editor when pinned to the taskbar.

## What’s in v1

- Frameless titlebar matching the text-editor (brand mark, drag region, min / max / close)
- Dark default theme; **View → Light Mode** (or Ctrl+M) with local persistence
- Real IPython over a PTY (`portable-pty` + xterm.js)
- Stdlib math preload: `math`, selected `cmath`, and `statistics` helpers in the interactive namespace

## Still to do

- Distinct polished app icon / AppUserModelID tuning for Windows taskbar pinning
- NSIS packaging smoke-test on a Windows machine
- Optional later: numpy/sympy opt-in, degrees mode, history polish

## Layout

```
index.html          # frameless shell + titlebar + terminal host
src/                # theme, chrome, xterm ↔ PTY bridge
src-tauri/          # Tauri 2 app + portable-pty IPython session
python/preload.py   # math namespace injected at IPython startup
```
