#pragma once

#include "eval/session.hpp"

#include <memory>
#include <windows.h>

namespace calc::ui {

inline constexpr wchar_t kWindowClass[] = L"Calculator.Win32.Phase1";
inline constexpr wchar_t kWindowTitle[] = L"Calculator";
inline constexpr wchar_t kWwMainStartProp[] = L"Calculator.WwMainStartQpc";
inline constexpr int kEditId = 1001;
inline constexpr int kResultId = 1002;
// External harness / automation:
//   PostMessage(main, kMsgEvaluate, 0, 0) → evaluate current EDIT text
//   WM_COPYDATA with dwData=kCopyDataEval → evaluate UTF-16 payload (preferred)
inline constexpr UINT kMsgEvaluate = WM_APP + 1;
inline constexpr ULONG_PTR kCopyDataEval = 0x43414C43;  // 'CALC'

struct AppState {
  HWND edit = nullptr;
  HWND result = nullptr;
  HFONT font = nullptr;
  WNDPROC edit_prev_proc = nullptr;
  // Constructed on first Enter only — never on the launch path.
  std::unique_ptr<calc::eval::Session> session;
};

// Creates the main window + child controls. Caller owns the returned HWND.
HWND CreateMainWindow(HINSTANCE instance, int show_cmd, AppState* state);

// Standard WndProc for the main frame.
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

}  // namespace calc::ui
