#include "eval/value.hpp"

#include <cstdio>
#include <limits>
#include <sstream>

namespace calc::eval {
namespace {

void AppendValue(std::ostringstream& os, const Value& v) {
  switch (v.kind) {
    case ValueKind::Int:
      os << v.i;
      break;
    case ValueKind::Float: {
      char buf[64];
      std::snprintf(buf, sizeof(buf), "%.15g", v.f);
      os << buf;
      break;
    }
    case ValueKind::List: {
      os << '[';
      for (size_t i = 0; i < v.list.size(); ++i) {
        if (i) os << ", ";
        AppendValue(os, v.list[i]);
      }
      os << ']';
      break;
    }
  }
}

}  // namespace

std::string FormatValue(const Value& v) {
  std::ostringstream os;
  AppendValue(os, v);
  return os.str();
}

bool CheckedAdd(std::int64_t a, std::int64_t b, std::int64_t* out, EvalError* err) {
  if ((b > 0 && a > (std::numeric_limits<std::int64_t>::max)() - b) ||
      (b < 0 && a < (std::numeric_limits<std::int64_t>::min)() - b)) {
    if (err) err->message = "integer overflow in addition";
    return false;
  }
  *out = a + b;
  return true;
}

bool CheckedSub(std::int64_t a, std::int64_t b, std::int64_t* out, EvalError* err) {
  if ((b > 0 && a < (std::numeric_limits<std::int64_t>::min)() + b) ||
      (b < 0 && a > (std::numeric_limits<std::int64_t>::max)() + b)) {
    if (err) err->message = "integer overflow in subtraction";
    return false;
  }
  *out = a - b;
  return true;
}

bool CheckedMul(std::int64_t a, std::int64_t b, std::int64_t* out, EvalError* err) {
  if (a == 0 || b == 0) {
    *out = 0;
    return true;
  }
  if ((a == -1 && b == (std::numeric_limits<std::int64_t>::min)()) ||
      (b == -1 && a == (std::numeric_limits<std::int64_t>::min)())) {
    if (err) err->message = "integer overflow in multiplication";
    return false;
  }
  if (a > 0) {
    if (b > 0) {
      if (a > (std::numeric_limits<std::int64_t>::max)() / b) {
        if (err) err->message = "integer overflow in multiplication";
        return false;
      }
    } else if (b < (std::numeric_limits<std::int64_t>::min)() / a) {
      if (err) err->message = "integer overflow in multiplication";
      return false;
    }
  } else {
    if (b > 0) {
      if (a < (std::numeric_limits<std::int64_t>::min)() / b) {
        if (err) err->message = "integer overflow in multiplication";
        return false;
      }
    } else if (b < (std::numeric_limits<std::int64_t>::max)() / a) {
      if (err) err->message = "integer overflow in multiplication";
      return false;
    }
  }
  *out = a * b;
  return true;
}

}  // namespace calc::eval
