#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace calc::eval {

enum class ValueKind { Int, Float, List };

struct Value {
  ValueKind kind = ValueKind::Int;
  std::int64_t i = 0;
  double f = 0.0;
  std::vector<Value> list;

  static Value Int(std::int64_t v) {
    Value out;
    out.kind = ValueKind::Int;
    out.i = v;
    return out;
  }

  static Value Float(double v) {
    Value out;
    out.kind = ValueKind::Float;
    out.f = v;
    return out;
  }

  static Value List(std::vector<Value> items) {
    Value out;
    out.kind = ValueKind::List;
    out.list = std::move(items);
    return out;
  }

  bool is_number() const { return kind == ValueKind::Int || kind == ValueKind::Float; }
  double as_float() const { return kind == ValueKind::Float ? f : static_cast<double>(i); }
};

struct EvalError {
  std::string message;
};

std::string FormatValue(const Value& v);

// Checked integer arithmetic. On overflow sets err and returns false.
bool CheckedAdd(std::int64_t a, std::int64_t b, std::int64_t* out, EvalError* err);
bool CheckedSub(std::int64_t a, std::int64_t b, std::int64_t* out, EvalError* err);
bool CheckedMul(std::int64_t a, std::int64_t b, std::int64_t* out, EvalError* err);

}  // namespace calc::eval
