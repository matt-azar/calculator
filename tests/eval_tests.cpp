#include "eval/session.hpp"
#include "eval/parse.hpp"

#include <cmath>
#include <iostream>
#include <string>
#include <unordered_map>

using calc::eval::EvaluateSource;
using calc::eval::Session;
using calc::eval::Value;
using calc::eval::ValueKind;

namespace {

int g_failed = 0;

void Expect(bool cond, const char* msg) {
  if (!cond) {
    std::cerr << "FAIL: " << msg << "\n";
    ++g_failed;
  } else {
    std::cout << "ok  " << msg << "\n";
  }
}

void ExpectInt(const Value& v, long long want, const char* msg) {
  Expect(v.kind == ValueKind::Int && v.i == want, msg);
}

void ExpectFloatNear(const Value& v, double want, double eps, const char* msg) {
  Expect(v.is_number() && std::fabs(v.as_float() - want) <= eps, msg);
}

Value Eval(const std::string& src, std::unordered_map<std::string, Value>* env) {
  auto pr = EvaluateSource(src, env);
  if (!pr.ok) {
    std::cerr << "eval error for '" << src << "': " << pr.error.message << "\n";
    ++g_failed;
    return Value::Int(0);
  }
  return pr.value;
}

}  // namespace

int main() {
  std::unordered_map<std::string, Value> env;

  // Python ** precedence must-pass.
  ExpectInt(Eval("-2**2", &env), -4, "-2**2 == -4");
  ExpectFloatNear(Eval("2**-3", &env), 0.125, 1e-12, "2**-3 == 0.125");
  ExpectInt(Eval("2**3**2", &env), 512, "2**3**2 == 512");

  // Smoke.
  ExpectFloatNear(Eval("sin(pi/2)", &env), 1.0, 1e-12, "sin(pi/2) == 1");
  ExpectFloatNear(Eval("mean([1, 2, 3, 4])", &env), 2.5, 1e-12, "mean([1,2,3,4]) == 2.5");

  Session session;
  std::string result;
  std::string error;
  Expect(session.EvalLine("x = 2**10", &result, &error), "assign x = 2**10 succeeds");
  Expect(result == "1024", "x = 2**10 formats 1024");
  Expect(session.EvalLine("x + 1", &result, &error), "x + 1 succeeds");
  Expect(result == "1025", "x + 1 == 1025");
  Expect(session.EvalLine("_", &result, &error) && result == "1025", "_ is last result");

  // History Up/Down (newest first: "_", then "x + 1", then assign).
  std::string line;
  Expect(session.HistoryUp(&line) && line == "_", "history up to _");
  Expect(session.HistoryUp(&line) && line == "x + 1", "history up to x + 1");
  Expect(session.HistoryUp(&line) && line == "x = 2**10", "history up to assign");
  Expect(session.HistoryDown(&line) && line == "x + 1", "history down");

  // Domain / overflow messages.
  Expect(!session.EvalLine("1/0", &result, &error) && error.find("zero") != std::string::npos,
         "division by zero errors");
  Expect(!session.EvalLine("sqrt(-1)", &result, &error), "sqrt(-1) domain error");

  // More arithmetic.
  env.clear();
  ExpectInt(Eval("2 + 3 * 4", &env), 14, "2 + 3 * 4 == 14");
  ExpectInt(Eval("(2 + 3) * 4", &env), 20, "(2 + 3) * 4 == 20");
  ExpectFloatNear(Eval("10 / 4", &env), 2.5, 1e-12, "10 / 4 == 2.5");

  if (g_failed) {
    std::cerr << g_failed << " test(s) failed\n";
    return 1;
  }
  std::cout << "All eval tests passed\n";
  return 0;
}
