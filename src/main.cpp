#include "ui/window.hpp"

#include <windows.h>

// Show a focused EDIT as fast as possible.
// Evaluator/session are constructed on first Enter only — not on this path.
int APIENTRY wWinMain(
    _In_ HINSTANCE instance,
    _In_opt_ HINSTANCE /*prev*/,
    _In_ LPWSTR /*cmd_line*/,
    _In_ int show_cmd) {
  // First statement: high-resolution timestamp for process vs app-setup split.
  LARGE_INTEGER wwmain_start{};
  QueryPerformanceCounter(&wwmain_start);

  calc::ui::AppState state{};
  HWND hwnd = calc::ui::CreateMainWindow(instance, show_cmd, &state);
  if (!hwnd) {
    MessageBoxW(nullptr, L"Failed to create Calculator window.", L"Calculator", MB_ICONERROR);
    return 1;
  }

  // Expose wWinMain-entry QPC ticks to the external launch harness.
  SetPropW(hwnd, calc::ui::kWwMainStartProp, reinterpret_cast<HANDLE>(wwmain_start.QuadPart));

  MSG msg{};
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  RemovePropW(hwnd, calc::ui::kWwMainStartProp);
  return static_cast<int>(msg.wParam);
}
