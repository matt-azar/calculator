#pragma once

#include <windows.h>

namespace calc::ui {

inline constexpr wchar_t kWindowClass[] = L"Calculator.Win32.Phase1";
inline constexpr wchar_t kWindowTitle[] = L"Calculator";
inline constexpr wchar_t kWwMainStartProp[] = L"Calculator.WwMainStartQpc";
inline constexpr int kEditId = 1001;
inline constexpr int kResultId = 1002;

struct AppState {
  HWND edit = nullptr;
  HWND result = nullptr;
  HFONT font = nullptr;
};

// Creates the main window + child controls. Caller owns the returned HWND.
HWND CreateMainWindow(HINSTANCE instance, int show_cmd, AppState* state);

// Standard WndProc for the main frame.
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

}  // namespace calc::ui
