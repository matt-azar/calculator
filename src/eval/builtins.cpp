#include "eval/builtins.hpp"

#include <algorithm>
#include <cmath>
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

}  // namespace

bool IsBuiltinName(const std::string& name) {
  static const std::unordered_set<std::string> kNames = {
      "sin", "cos", "tan", "asin", "acos", "atan", "atan2",
      "sqrt", "exp", "log", "ln", "log10", "log2", "abs",
      "floor", "ceil", "round",
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
  if (name == "sqrt") return unary([](double x) { return std::sqrt(x); });
  if (name == "exp") return unary([](double x) { return std::exp(x); });
  if (name == "log" || name == "ln") return unary([](double x) { return std::log(x); });
  if (name == "log10") return unary([](double x) { return std::log10(x); });
  if (name == "log2") return unary([](double x) { return std::log2(x); });
  if (name == "floor") return unary([](double x) { return std::floor(x); });
  if (name == "ceil") return unary([](double x) { return std::ceil(x); });
  if (name == "round") return unary([](double x) { return std::round(x); });

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
