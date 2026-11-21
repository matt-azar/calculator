#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr wchar_t kWindowClass[] = L"Calculator.Win32.Phase1";
constexpr wchar_t kWwMainStartProp[] = L"Calculator.WwMainStartQpc";
constexpr int kEditId = 1001;
constexpr int kResultId = 1002;
constexpr UINT kMsgEvaluate = WM_APP + 1;
constexpr ULONG_PTR kCopyDataEval = 0x43414C43;  // 'CALC'

enum class Mode { Keystroke, FirstResult };

struct Sample {
  double process_ms = 0;
  double visible_ms = 0;
  double keystroke_ms = 0;   // first char in EDIT (keystroke mode)
  double result_ms = 0;      // launch → "2" in result (first-result mode)
  bool ok = false;
  std::wstring error;
};

struct Options {
  std::wstring exe_path;
  int runs = 21;
  int settle_ms = 250;
  wchar_t character = L'a';
  std::wstring out_csv;
  bool keep_alive = false;
  Mode mode = Mode::Keystroke;
};

double NowMs() {
  using clock = std::chrono::steady_clock;
  static const auto t0 = clock::now();
  return std::chrono::duration<double, std::milli>(clock::now() - t0).count();
}

std::wstring QuoteArg(const std::wstring& s) {
  if (s.find_first_of(L" \t\"") == std::wstring::npos) {
    return s;
  }
  std::wstring out = L"\"";
  for (wchar_t c : s) {
    if (c == L'"') {
      out += L"\\\"";
    } else {
      out += c;
    }
  }
  out += L'"';
  return out;
}

struct EnumCtx {
  DWORD pid = 0;
  HWND hwnd = nullptr;
};

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lparam) {
  auto* ctx = reinterpret_cast<EnumCtx*>(lparam);
  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd, &pid);
  if (pid != ctx->pid) {
    return TRUE;
  }
  wchar_t cls[256]{};
  if (GetClassNameW(hwnd, cls, 256) == 0) {
    return TRUE;
  }
  if (wcscmp(cls, kWindowClass) != 0) {
    return TRUE;
  }
  if (!IsWindowVisible(hwnd)) {
    return TRUE;
  }
  ctx->hwnd = hwnd;
  return FALSE;
}

HWND FindCalculatorWindow(DWORD pid, DWORD timeout_ms) {
  const DWORD start = GetTickCount();
  while (GetTickCount() - start < timeout_ms) {
    EnumCtx ctx{pid, nullptr};
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&ctx));
    if (ctx.hwnd) {
      return ctx.hwnd;
    }
    Sleep(1);
  }
  return nullptr;
}

HWND FindEditChild(HWND root) {
  HWND edit = GetDlgItem(root, kEditId);
  if (edit) {
    return edit;
  }
  return FindWindowExW(root, nullptr, L"EDIT", nullptr);
}

HWND FindResultChild(HWND root) {
  return GetDlgItem(root, kResultId);
}

bool EditContains(HWND edit, wchar_t ch) {
  wchar_t buf[64]{};
  const int n = GetWindowTextW(edit, buf, 64);
  for (int i = 0; i < n; ++i) {
    if (buf[i] == ch) {
      return true;
    }
  }
  return false;
}

bool WaitForEditContains(HWND edit, wchar_t ch, DWORD timeout_ms) {
  const DWORD start = GetTickCount();
  while (GetTickCount() - start < timeout_ms) {
    if (EditContains(edit, ch)) {
      return true;
    }
    Sleep(1);
  }
  return false;
}

bool WindowTextContains(HWND hwnd, const wchar_t* needle) {
  const int len = GetWindowTextLengthW(hwnd);
  if (len <= 0) return false;
  std::wstring buf(static_cast<size_t>(len) + 1, L'\0');
  GetWindowTextW(hwnd, buf.data(), len + 1);
  buf.resize(static_cast<size_t>(len));
  return buf.find(needle) != std::wstring::npos;
}

bool WaitForTextContains(HWND hwnd, const wchar_t* needle, DWORD timeout_ms) {
  const DWORD start = GetTickCount();
  while (GetTickCount() - start < timeout_ms) {
    if (WindowTextContains(hwnd, needle)) {
      return true;
    }
    Sleep(1);
  }
  return false;
}

std::wstring ResultFilePath(const std::wstring& exe_path) {
  std::wstring path = exe_path;
  const size_t slash = path.find_last_of(L"\\/");
  if (slash != std::wstring::npos) {
    path.resize(slash + 1);
  } else {
    path.clear();
  }
  path += L"last_result.txt";
  return path;
}

bool WaitForResultFileEquals(const std::wstring& path, const char* want, DWORD timeout_ms) {
  const DWORD start = GetTickCount();
  while (GetTickCount() - start < timeout_ms) {
    FILE* f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"rb") == 0 && f) {
      char buf[64]{};
      size_t n = fread(buf, 1, sizeof(buf) - 1, f);
      fclose(f);
      while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r' || buf[n - 1] == ' ')) {
        --n;
      }
      buf[n] = 0;
      if (std::strcmp(buf, want) == 0) {
        return true;
      }
    }
    Sleep(1);
  }
  return false;
}

bool DeliverCharToEdit(HWND hwnd, HWND edit, wchar_t ch) {
  AllowSetForegroundWindow(ASFW_ANY);
  DWORD target_tid = GetWindowThreadProcessId(hwnd, nullptr);
  DWORD self_tid = GetCurrentThreadId();
  BOOL attached = FALSE;
  if (target_tid != 0 && target_tid != self_tid) {
    attached = AttachThreadInput(self_tid, target_tid, TRUE);
  }

  BringWindowToTop(hwnd);
  ShowWindow(hwnd, SW_SHOW);
  SetForegroundWindow(hwnd);
  SetFocus(edit);

  INPUT inputs[2]{};
  inputs[0].type = INPUT_KEYBOARD;
  inputs[0].ki.wScan = ch;
  inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;
  inputs[1].type = INPUT_KEYBOARD;
  inputs[1].ki.wScan = ch;
  inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
  SendInput(2, inputs, sizeof(INPUT));

  if (!EditContains(edit, ch)) {
    wchar_t text[2] = {ch, L'\0'};
    SendMessageW(edit, EM_SETSEL, 0, -1);
    SendMessageW(edit, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text));
  }
  if (!EditContains(edit, ch)) {
    SendMessageW(edit, WM_CHAR, static_cast<WPARAM>(ch), 0);
  }
  if (!EditContains(edit, ch)) {
    wchar_t text[2] = {ch, L'\0'};
    SetWindowTextW(edit, text);
  }

  if (attached) {
    AttachThreadInput(self_tid, target_tid, FALSE);
  }
  return true;
}

bool SubmitExpression(HWND hwnd, HWND /*edit*/, const wchar_t* expr) {
  // WM_COPYDATA is reliable cross-process: payload is the expression to evaluate.
  const size_t nchars = wcslen(expr) + 1;
  COPYDATASTRUCT cds{};
  cds.dwData = kCopyDataEval;
  cds.cbData = static_cast<DWORD>(nchars * sizeof(wchar_t));
  cds.lpData = const_cast<wchar_t*>(expr);
  const LRESULT r = SendMessageW(hwnd, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds));
  (void)kMsgEvaluate;
  return r != FALSE;
}

void CloseSample(HWND hwnd, HANDLE process, bool keep_alive) {
  if (!keep_alive) {
    PostMessageW(hwnd, WM_CLOSE, 0, 0);
    WaitForSingleObject(process, 3000);
    TerminateProcess(process, 0);
  }
  CloseHandle(process);
}

Sample RunOnce(const Options& opt) {
  Sample sample;

  STARTUPINFOW si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};

  std::wstring cmd = QuoteArg(opt.exe_path);
  std::vector<wchar_t> cmdline(cmd.begin(), cmd.end());
  cmdline.push_back(L'\0');

  const double t0 = NowMs();
  const BOOL created = CreateProcessW(
      opt.exe_path.c_str(),
      cmdline.data(),
      nullptr,
      nullptr,
      FALSE,
      0,
      nullptr,
      nullptr,
      &si,
      &pi);

  if (!created) {
    sample.error = L"CreateProcessW failed (" + std::to_wstring(GetLastError()) + L")";
    return sample;
  }

  sample.process_ms = NowMs() - t0;
  CloseHandle(pi.hThread);

  HWND hwnd = FindCalculatorWindow(pi.dwProcessId, 10000);
  if (!hwnd) {
    sample.error = L"Timed out waiting for visible window";
    TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hProcess);
    return sample;
  }
  sample.visible_ms = NowMs() - t0;
  (void)kWwMainStartProp;

  HWND edit = FindEditChild(hwnd);
  if (!edit) {
    sample.error = L"EDIT control not found";
    TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hProcess);
    return sample;
  }

  if (opt.mode == Mode::FirstResult) {
    const std::wstring result_path = ResultFilePath(opt.exe_path);
    DeleteFileW(result_path.c_str());
    if (!SubmitExpression(hwnd, edit, L"1+1")) {
      sample.error = L"failed to submit 1+1";
      TerminateProcess(pi.hProcess, 1);
      CloseHandle(pi.hProcess);
      return sample;
    }
    if (!WaitForResultFileEquals(result_path, "2", 5000)) {
      sample.error = L"result file never became '2'";
      TerminateProcess(pi.hProcess, 1);
      CloseHandle(pi.hProcess);
      return sample;
    }
    sample.result_ms = NowMs() - t0;
    sample.ok = true;
    CloseSample(hwnd, pi.hProcess, opt.keep_alive);
    return sample;
  }

  if (!DeliverCharToEdit(hwnd, edit, opt.character)) {
    sample.error = L"Failed to deliver probe character";
    TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hProcess);
    return sample;
  }

  if (!WaitForEditContains(edit, opt.character, 5000)) {
    sample.error = L"Character never appeared in EDIT";
    TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hProcess);
    return sample;
  }

  sample.keystroke_ms = NowMs() - t0;
  sample.ok = true;
  CloseSample(hwnd, pi.hProcess, opt.keep_alive);
  return sample;
}

double Percentile(std::vector<double> values, double p) {
  if (values.empty()) {
    return 0;
  }
  std::sort(values.begin(), values.end());
  const double rank = (p / 100.0) * static_cast<double>(values.size() - 1);
  const size_t lo = static_cast<size_t>(std::floor(rank));
  const size_t hi = static_cast<size_t>(std::ceil(rank));
  if (lo == hi) {
    return values[lo];
  }
  const double w = rank - static_cast<double>(lo);
  return values[lo] * (1.0 - w) + values[hi] * w;
}

double Median(std::vector<double> values) {
  return Percentile(std::move(values), 50.0);
}

void PrintUsage() {
  std::fwprintf(
      stderr,
      L"Usage: launch_bench --exe <calculator.exe> [--mode keystroke|first-result] "
      L"[--runs N] [--char C] [--out results.csv]\n");
}

bool ParseArgs(int argc, wchar_t** argv, Options* opt) {
  for (int i = 1; i < argc; ++i) {
    const std::wstring a = argv[i];
    auto need = [&](const wchar_t* name) -> const wchar_t* {
      if (i + 1 >= argc) {
        std::fwprintf(stderr, L"Missing value for %s\n", name);
        return nullptr;
      }
      return argv[++i];
    };

    if (a == L"--exe") {
      const wchar_t* v = need(L"--exe");
      if (!v) return false;
      opt->exe_path = v;
    } else if (a == L"--runs") {
      const wchar_t* v = need(L"--runs");
      if (!v) return false;
      opt->runs = _wtoi(v);
    } else if (a == L"--char") {
      const wchar_t* v = need(L"--char");
      if (!v || !*v) return false;
      opt->character = v[0];
    } else if (a == L"--out") {
      const wchar_t* v = need(L"--out");
      if (!v) return false;
      opt->out_csv = v;
    } else if (a == L"--mode") {
      const wchar_t* v = need(L"--mode");
      if (!v) return false;
      if (wcscmp(v, L"keystroke") == 0) {
        opt->mode = Mode::Keystroke;
      } else if (wcscmp(v, L"first-result") == 0) {
        opt->mode = Mode::FirstResult;
      } else {
        std::fwprintf(stderr, L"Unknown mode: %s\n", v);
        return false;
      }
    } else if (a == L"--keep") {
      opt->keep_alive = true;
    } else if (a == L"--help" || a == L"-h") {
      PrintUsage();
      return false;
    } else {
      std::fwprintf(stderr, L"Unknown arg: %s\n", a.c_str());
      PrintUsage();
      return false;
    }
  }
  if (opt->exe_path.empty() || opt->runs < 1) {
    PrintUsage();
    return false;
  }
  return true;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  Options opt;
  if (!ParseArgs(argc, argv, &opt)) {
    return 2;
  }

  std::vector<Sample> samples;
  samples.reserve(static_cast<size_t>(opt.runs));

  const wchar_t* mode_name = opt.mode == Mode::FirstResult ? L"first-result" : L"keystroke";
  std::wprintf(L"launch_bench: exe=%s runs=%d mode=%s\n", opt.exe_path.c_str(), opt.runs, mode_name);
  for (int i = 0; i < opt.runs; ++i) {
    Sample s = RunOnce(opt);
    samples.push_back(s);
    if (s.ok) {
      if (opt.mode == Mode::FirstResult) {
        std::wprintf(
            L"[%02d] process=%.2f ms  visible=%.2f ms  first_result=%.2f ms\n",
            i + 1,
            s.process_ms,
            s.visible_ms,
            s.result_ms);
      } else {
        std::wprintf(
            L"[%02d] process=%.2f ms  visible=%.2f ms  keystroke=%.2f ms\n",
            i + 1,
            s.process_ms,
            s.visible_ms,
            s.keystroke_ms);
      }
    } else {
      std::wprintf(L"[%02d] FAIL: %s\n", i + 1, s.error.c_str());
    }
    if (i + 1 < opt.runs) {
      Sleep(static_cast<DWORD>(opt.settle_ms));
    }
  }

  std::vector<double> process, visible, keystroke, result;
  for (const auto& s : samples) {
    if (!s.ok) continue;
    process.push_back(s.process_ms);
    visible.push_back(s.visible_ms);
    if (opt.mode == Mode::FirstResult) {
      result.push_back(s.result_ms);
    } else {
      keystroke.push_back(s.keystroke_ms);
    }
  }

  std::wprintf(L"\nSuccessful samples: %zu / %d\n", process.size(), opt.runs);
  if (!process.empty()) {
    std::wprintf(
        L"process_create_ms     median=%.2f  p95=%.2f\n",
        Median(process),
        Percentile(process, 95.0));
    std::wprintf(
        L"first_visible_ms      median=%.2f  p95=%.2f\n",
        Median(visible),
        Percentile(visible, 95.0));
    if (opt.mode == Mode::FirstResult) {
      std::wprintf(
          L"first_result_ms       median=%.2f  p95=%.2f\n",
          Median(result),
          Percentile(result, 95.0));
    } else {
      std::wprintf(
          L"first_keystroke_ms    median=%.2f  p95=%.2f\n",
          Median(keystroke),
          Percentile(keystroke, 95.0));
    }
  }

  if (!opt.out_csv.empty()) {
    FILE* f = nullptr;
    if (_wfopen_s(&f, opt.out_csv.c_str(), L"wb") == 0 && f) {
      fputs("\xEF\xBB\xBF", f);
      if (opt.mode == Mode::FirstResult) {
        fputs("index,ok,process_ms,visible_ms,result_ms,error\n", f);
      } else {
        fputs("index,ok,process_ms,visible_ms,keystroke_ms,error\n", f);
      }
      for (size_t i = 0; i < samples.size(); ++i) {
        const auto& s = samples[i];
        std::string err;
        for (wchar_t c : s.error) {
          if (c < 128) err.push_back(static_cast<char>(c));
          else err.push_back('?');
        }
        if (opt.mode == Mode::FirstResult) {
          std::fprintf(
              f,
              "%zu,%d,%.3f,%.3f,%.3f,\"%s\"\n",
              i + 1,
              s.ok ? 1 : 0,
              s.process_ms,
              s.visible_ms,
              s.result_ms,
              err.c_str());
        } else {
          std::fprintf(
              f,
              "%zu,%d,%.3f,%.3f,%.3f,\"%s\"\n",
              i + 1,
              s.ok ? 1 : 0,
              s.process_ms,
              s.visible_ms,
              s.keystroke_ms,
              err.c_str());
        }
      }
      fclose(f);
      std::wprintf(L"Wrote %s\n", opt.out_csv.c_str());
    }
  }

  return process.empty() ? 1 : 0;
}
