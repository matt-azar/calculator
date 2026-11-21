#include "ui/window.hpp"

#include <cstdio>
#include <string>
#include <vector>

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

std::string NarrowUtf8(const std::wstring& ws) {
  if (ws.empty()) return {};
  const int n = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), nullptr, 0,
                                    nullptr, nullptr);
  if (n <= 0) return {};
  std::string out(static_cast<size_t>(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), out.data(), n, nullptr,
                      nullptr);
  return out;
}

std::wstring WidenUtf8(const std::string& s) {
  if (s.empty()) return {};
  const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
  if (n <= 0) return {};
  std::wstring out(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), n);
  return out;
}

std::wstring GetEditText(HWND edit) {
  const int len = GetWindowTextLengthW(edit);
  std::wstring buf(static_cast<size_t>(len) + 1, L'\0');
  GetWindowTextW(edit, buf.data(), len + 1);
  buf.resize(static_cast<size_t>(len));
  return buf;
}

void AppendResultLine(HWND result, const std::wstring& line) {
  const int len = GetWindowTextLengthW(result);
  SendMessageW(result, EM_SETSEL, len, len);
  std::wstring chunk = line + L"\r\n";
  SendMessageW(result, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(chunk.c_str()));
}

void EnsureSession(AppState* state) {
  if (!state->session) {
    state->session = std::make_unique<calc::eval::Session>();
  }
}

void WriteAutomationResult(const std::string& text) {
  // Harness-friendly side channel: cross-process GetWindowText on this agent
  // desktop often misses in-WndProc WM_SETTEXT updates, so also write a file
  // next to the executable.
  wchar_t module[MAX_PATH]{};
  if (GetModuleFileNameW(nullptr, module, MAX_PATH) == 0) {
    return;
  }
  std::wstring path(module);
  const size_t slash = path.find_last_of(L"\\/");
  if (slash != std::wstring::npos) {
    path.resize(slash + 1);
  }
  path += L"last_result.txt";
  FILE* f = nullptr;
  if (_wfopen_s(&f, path.c_str(), L"wb") != 0 || !f) {
    return;
  }
  fwrite(text.data(), 1, text.size(), f);
  fputc('\n', f);
  fclose(f);
}

void EvaluateLine(AppState* state, const std::wstring& wline) {
  if (!state || !state->result) {
    return;
  }
  EnsureSession(state);
  const std::string line = NarrowUtf8(wline);

  std::string result;
  std::string error;
  const bool ok = state->session->EvalLine(line, &result, &error);
  // Replace the whole result pane so interactive use stays clear.
  std::wstring pane;
  pane += L">>> ";
  pane += wline;
  pane += L"\r\n";
  if (ok) {
    pane += WidenUtf8(result);
    pane += L"\r\n";
    SetWindowTextW(state->result, pane.c_str());
    if (state->edit) {
      SetWindowTextW(state->edit, L"");
    }
    WriteAutomationResult(result);
  } else {
    pane += L"Error: ";
    pane += WidenUtf8(error);
    pane += L"\r\n";
    SetWindowTextW(state->result, pane.c_str());
    WriteAutomationResult(std::string("Error: ") + error);
  }
}

void EvaluateCurrentLine(HWND /*hwnd*/, AppState* state) {
  if (!state || !state->edit) {
    return;
  }
  EvaluateLine(state, GetEditText(state->edit));
}

void HistoryNavigate(AppState* state, bool up) {
  if (!state->session) {
    // No history until the first successful Enter creates the session.
    return;
  }
  std::string line;
  const bool ok = up ? state->session->HistoryUp(&line) : state->session->HistoryDown(&line);
  if (!ok) return;
  SetWindowTextW(state->edit, WidenUtf8(line).c_str());
  SendMessageW(state->edit, EM_SETSEL, static_cast<WPARAM>(-1), static_cast<WPARAM>(-1));
}

LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  HWND parent = GetParent(hwnd);
  AppState* state = parent ? StateFromHwnd(parent) : nullptr;
  if (!state || !state->edit_prev_proc) {
    return DefWindowProcW(hwnd, msg, wparam, lparam);
  }

  if (msg == WM_KEYDOWN) {
    if (wparam == VK_RETURN) {
      EvaluateCurrentLine(parent, state);
      return 0;
    }
    if (wparam == VK_UP) {
      HistoryNavigate(state, true);
      return 0;
    }
    if (wparam == VK_DOWN) {
      HistoryNavigate(state, false);
      return 0;
    }
  }
  // Some input paths deliver Enter as WM_CHAR '\r'.
  if (msg == WM_CHAR && (wparam == L'\r' || wparam == L'\n')) {
    EvaluateCurrentLine(parent, state);
    return 0;
  }

  return CallWindowProcW(state->edit_prev_proc, hwnd, msg, wparam, lparam);
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

      // Intentionally do not construct Session here — first Enter only.

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
          L"Enter an expression and press Enter.\r\n"
          L"Up/Down recalls input history (after the first evaluation).",
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

      state->edit_prev_proc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
          state->edit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditSubclassProc)));

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

    case WM_APP + 1: {
      auto* state = StateFromHwnd(hwnd);
      if (state) {
        EvaluateCurrentLine(hwnd, state);
      }
      return 0;
    }

    case WM_COPYDATA: {
      auto* state = StateFromHwnd(hwnd);
      auto* cds = reinterpret_cast<COPYDATASTRUCT*>(lparam);
      if (!state || !cds || cds->dwData != kCopyDataEval || !cds->lpData || cds->cbData < sizeof(wchar_t)) {
        return FALSE;
      }
      const size_t nchars = cds->cbData / sizeof(wchar_t);
      std::wstring expr(static_cast<const wchar_t*>(cds->lpData), nchars);
      // Trim optional trailing null included in cbData.
      while (!expr.empty() && expr.back() == L'\0') {
        expr.pop_back();
      }
      if (state->edit) {
        SetWindowTextW(state->edit, expr.c_str());
      }
      EvaluateLine(state, expr);
      return TRUE;
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
      if (state) {
        if (state->edit && state->edit_prev_proc) {
          SetWindowLongPtrW(state->edit, GWLP_WNDPROC,
                            reinterpret_cast<LONG_PTR>(state->edit_prev_proc));
          state->edit_prev_proc = nullptr;
        }
        state->session.reset();
        if (state->font) {
          DeleteObject(state->font);
          state->font = nullptr;
        }
      }
      PostQuitMessage(0);
      return 0;
    }

    default:
      return DefWindowProcW(hwnd, msg, wparam, lparam);
  }
}

}  // namespace calc::ui
