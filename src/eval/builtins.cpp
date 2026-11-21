#include "eval/builtins.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <unordered_set>

namespace calc::eval {
namespace {

bool NeedArity(const std::vector<Value>& args, size_t n, const char* name, EvalError* err) {
  if (args.size() != n) {
    if (err) err->message = std::string(name) + "() expects " + std::to_string(n) + " argument(s)";
    return false;
  }
  return true;
}

bool AsNumber(const Value& v, double* out, EvalError* err, const char* ctx) {
  if (!v.is_number()) {
    if (err) err->message = std::string(ctx) + " expects a number";
    return false;
  }
  *out = v.as_float();
  return true;
}

bool ListNumbers(const Value& v, std::vector<double>* out, EvalError* err, const char* ctx) {
  if (v.kind != ValueKind::List) {
    if (err) err->message = std::string(ctx) + " expects a list";
    return false;
  }
  out->clear();
  out->reserve(v.list.size());
  for (const auto& item : v.list) {
    if (!item.is_number()) {
      if (err) err->message = std::string(ctx) + " list items must be numbers";
      return false;
    }
    out->push_back(item.as_float());
  }
  return true;
}

Value FromFloat(double x) { return Value::Float(x); }

bool AsInt64(const Value& v, std::int64_t* out, EvalError* err, const char* ctx) {
  if (!v.is_number()) {
    if (err) err->message = std::string(ctx) + " expects a number";
    return false;
  }
  if (v.kind == ValueKind::Int) {
    *out = v.i;
    return true;
  }
  const double x = v.f;
  if (!std::isfinite(x) || std::floor(x) != x) {
    if (err) err->message = std::string(ctx) + " expects an integer";
    return false;
  }
  if (x > static_cast<double>((std::numeric_limits<std::int64_t>::max)()) ||
      x < static_cast<double>((std::numeric_limits<std::int64_t>::min)())) {
    if (err) err->message = std::string(ctx) + " argument too large";
    return false;
  }
  *out = static_cast<std::int64_t>(x);
  return true;
}

bool AsNonNegInt(const Value& v, std::int64_t* out, EvalError* err, const char* ctx) {
  if (!AsInt64(v, out, err, ctx)) return false;
  if (*out < 0) {
    if (err) err->message = std::string(ctx) + " requires n >= 0";
    return false;
  }
  return true;
}

bool FactorialImpl(std::int64_t n, Value* out, EvalError* err, const char* ctx) {
  if (n < 0) {
    if (err) err->message = std::string(ctx) + " requires n >= 0";
    return false;
  }
  // 20! fits in int64; 21! does not.
  if (n <= 20) {
    std::int64_t r = 1;
    for (std::int64_t i = 2; i <= n; ++i) r *= i;
    *out = Value::Int(r);
    return true;
  }
  // Double path up to ~170!.
  if (n > 170) {
    if (err) err->message = std::string(ctx) + " overflow";
    return false;
  }
  double r = 1.0;
  for (std::int64_t i = 2; i <= n; ++i) {
    r *= static_cast<double>(i);
    if (!std::isfinite(r)) {
      if (err) err->message = std::string(ctx) + " overflow";
      return false;
    }
  }
  *out = FromFloat(r);
  return true;
}

std::int64_t Gcd64(std::int64_t a, std::int64_t b) {
  if (a < 0) a = -a;
  if (b < 0) b = -b;
  while (b != 0) {
    const std::int64_t t = a % b;
    a = b;
    b = t;
  }
  return a;
}

bool CombImpl(std::int64_t n, std::int64_t k, Value* out, EvalError* err, const char* ctx) {
  if (k < 0 || n < 0) {
    if (err) err->message = std::string(ctx) + " requires n, k >= 0";
    return false;
  }
  if (k > n) {
    *out = Value::Int(0);
    return true;
  }
  if (k > n - k) k = n - k;
  // Multiply carefully; promote to double if int would overflow.
  std::int64_t r = 1;
  bool as_float = false;
  double rf = 1.0;
  for (std::int64_t i = 1; i <= k; ++i) {
    const std::int64_t num = n - k + i;
    if (!as_float) {
      // r = r * num / i, exact for comb.
      if (r > (std::numeric_limits<std::int64_t>::max)() / (num > 0 ? num : 1)) {
        as_float = true;
        rf = static_cast<double>(r) * static_cast<double>(num) / static_cast<double>(i);
        continue;
      }
      r = r * num / i;
    } else {
      rf = rf * static_cast<double>(num) / static_cast<double>(i);
      if (!std::isfinite(rf)) {
        if (err) err->message = std::string(ctx) + " overflow";
        return false;
      }
    }
  }
  if (as_float) {
    *out = FromFloat(rf);
  } else {
    *out = Value::Int(r);
  }
  return true;
}

bool PermImpl(std::int64_t n, std::int64_t k, Value* out, EvalError* err, const char* ctx) {
  if (k < 0 || n < 0) {
    if (err) err->message = std::string(ctx) + " requires n, k >= 0";
    return false;
  }
  if (k > n) {
    *out = Value::Int(0);
    return true;
  }
  std::int64_t r = 1;
  bool as_float = false;
  double rf = 1.0;
  for (std::int64_t i = 0; i < k; ++i) {
    const std::int64_t term = n - i;
    if (!as_float) {
      if (term != 0 && r > (std::numeric_limits<std::int64_t>::max)() / term) {
        as_float = true;
        rf = static_cast<double>(r) * static_cast<double>(term);
        continue;
      }
      r *= term;
    } else {
      rf *= static_cast<double>(term);
      if (!std::isfinite(rf)) {
        if (err) err->message = std::string(ctx) + " overflow";
        return false;
      }
    }
  }
  if (as_float) {
    *out = FromFloat(rf);
  } else {
    *out = Value::Int(r);
  }
  return true;
}

}  // namespace

bool IsBuiltinName(const std::string& name) {
  static const std::unordered_set<std::string> kNames = {
      // Trig / inverse / hyperbolic
      "sin", "cos", "tan", "asin", "acos", "atan", "atan2",
      "sinh", "cosh", "tanh", "asinh", "acosh", "atanh",
      // Exp / logs / roots
      "sqrt", "cbrt", "exp", "expm1", "log", "ln", "log10", "log2", "log1p",
      // Rounding / abs / sign
      "abs", "floor", "ceil", "round", "trunc", "sign", "sgn",
      // Angle conversion
      "degrees", "radians",
      // Combinatorics / number theory
      "factorial", "fact", "gcd", "lcm", "comb", "perm", "nCr", "nPr",
      // Misc numeric
      "hypot", "pow", "mod", "clamp",
      // Lists / stats
      "mean", "median", "stdev", "pstdev", "sum", "min", "max", "len",
  };
  return kNames.count(name) != 0;
}

bool CallBuiltin(const std::string& name, const std::vector<Value>& args, Value* out, EvalError* err) {
  auto unary = [&](auto fn) -> bool {
    if (!NeedArity(args, 1, name.c_str(), err)) return false;
    double x = 0;
    if (!AsNumber(args[0], &x, err, name.c_str())) return false;
    const double y = fn(x);
    if (!std::isfinite(y)) {
      if (err) err->message = name + " domain error";
      return false;
    }
    *out = FromFloat(y);
    return true;
  };

  if (name == "sin") return unary([](double x) { return std::sin(x); });
  if (name == "cos") return unary([](double x) { return std::cos(x); });
  if (name == "tan") return unary([](double x) { return std::tan(x); });
  if (name == "asin") return unary([](double x) { return std::asin(x); });
  if (name == "acos") return unary([](double x) { return std::acos(x); });
  if (name == "atan") return unary([](double x) { return std::atan(x); });
  if (name == "sinh") return unary([](double x) { return std::sinh(x); });
  if (name == "cosh") return unary([](double x) { return std::cosh(x); });
  if (name == "tanh") return unary([](double x) { return std::tanh(x); });
  if (name == "asinh") return unary([](double x) { return std::asinh(x); });
  if (name == "acosh") return unary([](double x) { return std::acosh(x); });
  if (name == "atanh") return unary([](double x) { return std::atanh(x); });
  if (name == "sqrt") return unary([](double x) { return std::sqrt(x); });
  if (name == "cbrt") return unary([](double x) { return std::cbrt(x); });
  if (name == "exp") return unary([](double x) { return std::exp(x); });
  if (name == "expm1") return unary([](double x) { return std::expm1(x); });
  if (name == "log" || name == "ln") return unary([](double x) { return std::log(x); });
  if (name == "log10") return unary([](double x) { return std::log10(x); });
  if (name == "log2") return unary([](double x) { return std::log2(x); });
  if (name == "log1p") return unary([](double x) { return std::log1p(x); });
  if (name == "floor") return unary([](double x) { return std::floor(x); });
  if (name == "ceil") return unary([](double x) { return std::ceil(x); });
  if (name == "round") return unary([](double x) { return std::round(x); });
  if (name == "trunc") return unary([](double x) { return std::trunc(x); });
  if (name == "degrees") return unary([](double x) { return x * (180.0 / 3.14159265358979323846); });
  if (name == "radians") return unary([](double x) { return x * (3.14159265358979323846 / 180.0); });

  if (name == "sign" || name == "sgn") {
    if (!NeedArity(args, 1, name.c_str(), err)) return false;
    double x = 0;
    if (!AsNumber(args[0], &x, err, name.c_str())) return false;
    if (x > 0) {
      *out = Value::Int(1);
    } else if (x < 0) {
      *out = Value::Int(-1);
    } else {
      *out = Value::Int(0);
    }
    return true;
  }

  if (name == "abs") {
    if (!NeedArity(args, 1, "abs", err)) return false;
    if (args[0].kind == ValueKind::Int) {
      if (args[0].i == (std::numeric_limits<std::int64_t>::min)()) {
        if (err) err->message = "integer overflow in abs";
        return false;
      }
      *out = Value::Int(args[0].i < 0 ? -args[0].i : args[0].i);
      return true;
    }
    return unary([](double x) { return std::fabs(x); });
  }

  if (name == "atan2") {
    if (!NeedArity(args, 2, "atan2", err)) return false;
    double y = 0, x = 0;
    if (!AsNumber(args[0], &y, err, "atan2") || !AsNumber(args[1], &x, err, "atan2")) return false;
    *out = FromFloat(std::atan2(y, x));
    return true;
  }

  if (name == "hypot") {
    if (!NeedArity(args, 2, "hypot", err)) return false;
    double x = 0, y = 0;
    if (!AsNumber(args[0], &x, err, "hypot") || !AsNumber(args[1], &y, err, "hypot")) return false;
    *out = FromFloat(std::hypot(x, y));
    return true;
  }

  if (name == "pow") {
    if (!NeedArity(args, 2, "pow", err)) return false;
    double x = 0, y = 0;
    if (!AsNumber(args[0], &x, err, "pow") || !AsNumber(args[1], &y, err, "pow")) return false;
    if (x == 0.0 && y < 0.0) {
      if (err) err->message = "0 cannot be raised to a negative power";
      return false;
    }
    const double r = std::pow(x, y);
    if (!std::isfinite(r)) {
      if (err) err->message = "pow domain/overflow error";
      return false;
    }
    *out = FromFloat(r);
    return true;
  }

  if (name == "mod") {
    if (!NeedArity(args, 2, "mod", err)) return false;
    double x = 0, y = 0;
    if (!AsNumber(args[0], &x, err, "mod") || !AsNumber(args[1], &y, err, "mod")) return false;
    if (y == 0.0) {
      if (err) err->message = "modulo by zero";
      return false;
    }
    if (args[0].kind == ValueKind::Int && args[1].kind == ValueKind::Int) {
      const std::int64_t a = args[0].i;
      const std::int64_t b = args[1].i;
      std::int64_t r = a % b;
      if (r != 0 && ((r < 0) != (b < 0))) r += b;
      *out = Value::Int(r);
      return true;
    }
    double r = std::fmod(x, y);
    if (r != 0.0 && ((r < 0) != (y < 0))) r += y;
    *out = FromFloat(r);
    return true;
  }

  if (name == "clamp") {
    if (!NeedArity(args, 3, "clamp", err)) return false;
    double x = 0, lo = 0, hi = 0;
    if (!AsNumber(args[0], &x, err, "clamp") || !AsNumber(args[1], &lo, err, "clamp") ||
        !AsNumber(args[2], &hi, err, "clamp")) {
      return false;
    }
    if (lo > hi) {
      if (err) err->message = "clamp() requires lo <= hi";
      return false;
    }
    *out = FromFloat(std::min(hi, std::max(lo, x)));
    return true;
  }

  if (name == "factorial" || name == "fact") {
    if (!NeedArity(args, 1, name.c_str(), err)) return false;
    std::int64_t n = 0;
    if (!AsNonNegInt(args[0], &n, err, name.c_str())) return false;
    return FactorialImpl(n, out, err, name.c_str());
  }

  if (name == "gcd") {
    if (!NeedArity(args, 2, "gcd", err)) return false;
    std::int64_t a = 0, b = 0;
    if (!AsInt64(args[0], &a, err, "gcd") || !AsInt64(args[1], &b, err, "gcd")) return false;
    *out = Value::Int(Gcd64(a, b));
    return true;
  }

  if (name == "lcm") {
    if (!NeedArity(args, 2, "lcm", err)) return false;
    std::int64_t a = 0, b = 0;
    if (!AsInt64(args[0], &a, err, "lcm") || !AsInt64(args[1], &b, err, "lcm")) return false;
    if (a == 0 || b == 0) {
      *out = Value::Int(0);
      return true;
    }
    const std::int64_t g = Gcd64(a, b);
    const std::int64_t aa = a < 0 ? -a : a;
    const std::int64_t bb = b < 0 ? -b : b;
    if (aa / g > (std::numeric_limits<std::int64_t>::max)() / bb) {
      if (err) err->message = "lcm overflow";
      return false;
    }
    *out = Value::Int((aa / g) * bb);
    return true;
  }

  if (name == "comb" || name == "nCr") {
    if (!NeedArity(args, 2, name.c_str(), err)) return false;
    std::int64_t n = 0, k = 0;
    if (!AsNonNegInt(args[0], &n, err, name.c_str()) || !AsNonNegInt(args[1], &k, err, name.c_str())) {
      return false;
    }
    return CombImpl(n, k, out, err, name.c_str());
  }

  if (name == "perm" || name == "nPr") {
    if (!NeedArity(args, 2, name.c_str(), err)) return false;
    std::int64_t n = 0, k = 0;
    if (!AsNonNegInt(args[0], &n, err, name.c_str()) || !AsNonNegInt(args[1], &k, err, name.c_str())) {
      return false;
    }
    return PermImpl(n, k, out, err, name.c_str());
  }

  if (name == "len") {
    if (!NeedArity(args, 1, "len", err)) return false;
    if (args[0].kind != ValueKind::List) {
      if (err) err->message = "len() expects a list";
      return false;
    }
    *out = Value::Int(static_cast<std::int64_t>(args[0].list.size()));
    return true;
  }

  if (name == "sum" || name == "mean" || name == "min" || name == "max" || name == "median" ||
      name == "stdev" || name == "pstdev") {
    if (!NeedArity(args, 1, name.c_str(), err)) return false;
    std::vector<double> xs;
    if (!ListNumbers(args[0], &xs, err, name.c_str())) return false;
    if (xs.empty() && (name == "mean" || name == "min" || name == "max" || name == "median" ||
                       name == "stdev" || name == "pstdev")) {
      if (err) err->message = name + "() of empty list";
      return false;
    }

    if (name == "sum") {
      const double s = std::accumulate(xs.begin(), xs.end(), 0.0);
      *out = FromFloat(s);
      return true;
    }
    if (name == "mean") {
      const double s = std::accumulate(xs.begin(), xs.end(), 0.0);
      *out = FromFloat(s / static_cast<double>(xs.size()));
      return true;
    }
    if (name == "min") {
      *out = FromFloat(*std::min_element(xs.begin(), xs.end()));
      return true;
    }
    if (name == "max") {
      *out = FromFloat(*std::max_element(xs.begin(), xs.end()));
      return true;
    }
    if (name == "median") {
      auto sorted = xs;
      std::sort(sorted.begin(), sorted.end());
      const size_t m = sorted.size() / 2;
      if (sorted.size() % 2 == 1) {
        *out = FromFloat(sorted[m]);
      } else {
        *out = FromFloat(0.5 * (sorted[m - 1] + sorted[m]));
      }
      return true;
    }
    // sample (stdev) / population (pstdev)
    const double mean = std::accumulate(xs.begin(), xs.end(), 0.0) / static_cast<double>(xs.size());
    double acc = 0.0;
    for (double x : xs) {
      const double d = x - mean;
      acc += d * d;
    }
    if (name == "stdev") {
      if (xs.size() < 2) {
        if (err) err->message = "stdev() requires at least 2 values";
        return false;
      }
      *out = FromFloat(std::sqrt(acc / static_cast<double>(xs.size() - 1)));
      return true;
    }
    *out = FromFloat(std::sqrt(acc / static_cast<double>(xs.size())));
    return true;
  }

  if (err) err->message = "unknown function '" + name + "'";
  return false;
}

}  // namespace calc::eval
