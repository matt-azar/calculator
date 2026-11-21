#include "ui/window.hpp"

#include <string>

namespace calc::ui {
namespace {

constexpr int kPad = 10;
constexpr int kEditHeight = 28;
constexpr int kMinWidth = 420;
constexpr int kMinHeight = 280;

void LayoutChildren(HWND hwnd, AppState* state) {
  if (!state || !state->edit || !state->result) {
    return;
  }

  RECT rc{};
  GetClientRect(hwnd, &rc);
  const int width = rc.right - rc.left;
  const int height = rc.bottom - rc.top;
  const int edit_w = (width > 2 * kPad) ? (width - 2 * kPad) : 0;
  const int edit_h = kEditHeight;
  const int result_top = kPad + edit_h + kPad;
  const int result_h = (height > result_top + kPad) ? (height - result_top - kPad) : 0;

  MoveWindow(state->edit, kPad, kPad, edit_w, edit_h, TRUE);
  MoveWindow(state->result, kPad, result_top, edit_w, result_h, TRUE);
}

void FocusEdit(AppState* state) {
  if (state && state->edit) {
    SetFocus(state->edit);
  }
}

AppState* StateFromHwnd(HWND hwnd) {
  return reinterpret_cast<AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

}  // namespace

HWND CreateMainWindow(HINSTANCE instance, int show_cmd, AppState* state) {
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = MainWndProc;
  wc.hInstance = instance;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  wc.lpszClassName = kWindowClass;
  wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  wc.hIconSm = wc.hIcon;

  ATOM atom = RegisterClassExW(&wc);
  if (!atom) {
    const DWORD err = GetLastError();
    if (err != ERROR_CLASS_ALREADY_EXISTS) {
      return nullptr;
    }
  }

  HWND hwnd = CreateWindowExW(
      0,
      kWindowClass,
      kWindowTitle,
      WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      560,
      360,
      nullptr,
      nullptr,
      instance,
      state);

  if (!hwnd) {
    return nullptr;
  }

  ShowWindow(hwnd, show_cmd);
  UpdateWindow(hwnd);
  FocusEdit(state);
  return hwnd;
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
    case WM_NCCREATE: {
      auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
      auto* state = static_cast<AppState*>(cs->lpCreateParams);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
      return TRUE;
    }

    case WM_CREATE: {
      auto* state = StateFromHwnd(hwnd);
      if (!state) {
        return -1;
      }

      state->font = CreateFontW(
          -16,
          0,
          0,
          0,
          FW_NORMAL,
          FALSE,
          FALSE,
          FALSE,
          DEFAULT_CHARSET,
          OUT_DEFAULT_PRECIS,
          CLIP_DEFAULT_PRECIS,
          CLEARTYPE_QUALITY,
          FIXED_PITCH | FF_MODERN,
          L"Consolas");

      state->edit = CreateWindowExW(
          WS_EX_CLIENTEDGE,
          L"EDIT",
          L"",
          WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_LEFT,
          0,
          0,
          0,
          0,
          hwnd,
          reinterpret_cast<HMENU>(static_cast<INT_PTR>(kEditId)),
          reinterpret_cast<LPCREATESTRUCTW>(lparam)->hInstance,
          nullptr);

      state->result = CreateWindowExW(
          WS_EX_CLIENTEDGE,
          L"EDIT",
          L"Phase 1 — expression result area (evaluator arrives in Phase 2).\r\n"
          L"Type, edit, and paste in the field above.",
          WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
          0,
          0,
          0,
          0,
          hwnd,
          reinterpret_cast<HMENU>(static_cast<INT_PTR>(kResultId)),
          reinterpret_cast<LPCREATESTRUCTW>(lparam)->hInstance,
          nullptr);

      if (!state->edit || !state->result) {
        return -1;
      }

      if (state->font) {
        SendMessageW(state->edit, WM_SETFONT, reinterpret_cast<WPARAM>(state->font), TRUE);
        SendMessageW(state->result, WM_SETFONT, reinterpret_cast<WPARAM>(state->font), TRUE);
      }

      LayoutChildren(hwnd, state);
      return 0;
    }

    case WM_SIZE: {
      LayoutChildren(hwnd, StateFromHwnd(hwnd));
      return 0;
    }

    case WM_GETMINMAXINFO: {
      auto* mmi = reinterpret_cast<MINMAXINFO*>(lparam);
      mmi->ptMinTrackSize.x = kMinWidth;
      mmi->ptMinTrackSize.y = kMinHeight;
      return 0;
    }

    case WM_ACTIVATE: {
      if (LOWORD(wparam) != WA_INACTIVE) {
        FocusEdit(StateFromHwnd(hwnd));
      }
      return 0;
    }

    case WM_SETFOCUS: {
      FocusEdit(StateFromHwnd(hwnd));
      return 0;
    }

    case WM_DESTROY: {
      auto* state = StateFromHwnd(hwnd);
      if (state && state->font) {
        DeleteObject(state->font);
        state->font = nullptr;
      }
      PostQuitMessage(0);
      return 0;
    }

    default:
      return DefWindowProcW(hwnd, msg, wparam, lparam);
  }
}

}  // namespace calc::ui
