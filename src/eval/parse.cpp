#include "eval/parse.hpp"

#include "eval/builtins.hpp"

#include <cmath>
#include <cstdlib>
#include <limits>

namespace calc::eval {
namespace {

class Parser {
 public:
  Parser(const std::vector<Token>& tokens, std::unordered_map<std::string, Value>* env)
      : tokens_(tokens), env_(env) {}

  ParseResult ParseStatement() {
    ParseResult out;
    if (Peek().kind == TokenKind::Name && Lookahead(1).kind == TokenKind::Equal) {
      const std::string name = Peek().text;
      if (name == "_") {
        out.error.message = "cannot assign to '_'";
        return out;
      }
      Advance();  // name
      Advance();  // =
      Value rhs;
      if (!ParseExpression(&rhs, &out.error)) return out;
      if (!AtEnd()) {
        out.error.message = "unexpected trailing tokens";
        return out;
      }
      (*env_)[name] = rhs;
      out.value = rhs;
      out.ok = true;
      return out;
    }

    Value v;
    if (!ParseExpression(&v, &out.error)) return out;
    if (!AtEnd()) {
      out.error.message = "unexpected trailing tokens";
      return out;
    }
    out.value = v;
    out.ok = true;
    return out;
  }

 private:
  const std::vector<Token>& tokens_;
  std::unordered_map<std::string, Value>* env_;
  size_t i_ = 0;

  const Token& Peek() const { return tokens_[i_]; }
  const Token& Lookahead(size_t n) const {
    const size_t idx = i_ + n;
    if (idx >= tokens_.size()) return tokens_.back();
    return tokens_[idx];
  }
  void Advance() {
    if (i_ + 1 < tokens_.size()) ++i_;
  }
  bool AtEnd() const { return Peek().kind == TokenKind::End; }

  bool ParseExpression(Value* out, EvalError* err) { return ParseSum(out, err); }

  bool ParseSum(Value* out, EvalError* err) {
    if (!ParseProduct(out, err)) return false;
    while (Peek().kind == TokenKind::Plus || Peek().kind == TokenKind::Minus) {
      const TokenKind op = Peek().kind;
      Advance();
      Value rhs;
      if (!ParseProduct(&rhs, err)) return false;
      if (!ApplyAddSub(*out, rhs, op == TokenKind::Plus, out, err)) return false;
    }
    return true;
  }

  bool ParseProduct(Value* out, EvalError* err) {
    if (!ParseFactor(out, err)) return false;
    while (Peek().kind == TokenKind::Star || Peek().kind == TokenKind::Slash ||
           Peek().kind == TokenKind::Percent) {
      const TokenKind op = Peek().kind;
      Advance();
      Value rhs;
      if (!ParseFactor(&rhs, err)) return false;
      if (op == TokenKind::Star) {
        if (!ApplyMul(*out, rhs, out, err)) return false;
      } else if (op == TokenKind::Slash) {
        if (!ApplyDiv(*out, rhs, out, err)) return false;
      } else {
        if (!ApplyMod(*out, rhs, out, err)) return false;
      }
    }
    return true;
  }

  // Python: factor = (+|-) factor | power
  bool ParseFactor(Value* out, EvalError* err) {
    if (Peek().kind == TokenKind::Plus) {
      Advance();
      return ParseFactor(out, err);
    }
    if (Peek().kind == TokenKind::Minus) {
      Advance();
      Value inner;
      if (!ParseFactor(&inner, err)) return false;
      return ApplyUnaryMinus(inner, out, err);
    }
    return ParsePower(out, err);
  }

  // power = atom ['!']* ['**' factor]  — postfix ! is factorial (binds tighter than **)
  bool ParsePower(Value* out, EvalError* err) {
    if (!ParseAtom(out, err)) return false;
    while (Peek().kind == TokenKind::Bang) {
      Advance();
      if (!CallBuiltin("factorial", {*out}, out, err)) return false;
    }
    if (Peek().kind == TokenKind::StarStar) {
      Advance();
      Value exp;
      if (!ParseFactor(&exp, err)) return false;
      return ApplyPow(*out, exp, out, err);
    }
    return true;
  }

  bool ParseAtom(Value* out, EvalError* err) {
    const Token& t = Peek();
    if (t.kind == TokenKind::Number) {
      Advance();
      if (t.number_is_float) {
        *out = Value::Float(t.number);
      } else {
        try {
          *out = Value::Int(std::stoll(t.text));
        } catch (...) {
          // Fallback for huge ints that still fit in double path.
          *out = Value::Float(t.number);
        }
      }
      return true;
    }

    if (t.kind == TokenKind::Name) {
      const std::string name = t.text;
      Advance();
      if (Peek().kind == TokenKind::LParen) {
        Advance();
        std::vector<Value> args;
        if (Peek().kind != TokenKind::RParen) {
          for (;;) {
            Value arg;
            if (!ParseExpression(&arg, err)) return false;
            args.push_back(std::move(arg));
            if (Peek().kind == TokenKind::Comma) {
              Advance();
              continue;
            }
            break;
          }
        }
        if (Peek().kind != TokenKind::RParen) {
          err->message = "expected ')'";
          return false;
        }
        Advance();
        if (IsBuiltinName(name)) {
          return CallBuiltin(name, args, out, err);
        }
        err->message = "unknown function '" + name + "'";
        return false;
      }

      if (name == "pi") {
        *out = Value::Float(3.14159265358979323846);
        return true;
      }
      if (name == "e") {
        *out = Value::Float(2.71828182845904523536);
        return true;
      }
      auto it = env_->find(name);
      if (it == env_->end()) {
        err->message = "undefined name '" + name + "'";
        return false;
      }
      *out = it->second;
      return true;
    }

    if (t.kind == TokenKind::LParen) {
      Advance();
      if (!ParseExpression(out, err)) return false;
      if (Peek().kind != TokenKind::RParen) {
        err->message = "expected ')'";
        return false;
      }
      Advance();
      return true;
    }

    if (t.kind == TokenKind::LBracket) {
      Advance();
      std::vector<Value> items;
      if (Peek().kind != TokenKind::RBracket) {
        for (;;) {
          Value item;
          if (!ParseExpression(&item, err)) return false;
          items.push_back(std::move(item));
          if (Peek().kind == TokenKind::Comma) {
            Advance();
            continue;
          }
          break;
        }
      }
      if (Peek().kind != TokenKind::RBracket) {
        err->message = "expected ']'";
        return false;
      }
      Advance();
      *out = Value::List(std::move(items));
      return true;
    }

    err->message = "expected expression";
    return false;
  }

  static bool ApplyUnaryMinus(const Value& v, Value* out, EvalError* err) {
    if (v.kind == ValueKind::Int) {
      if (v.i == (std::numeric_limits<std::int64_t>::min)()) {
        err->message = "integer overflow in negation";
        return false;
      }
      *out = Value::Int(-v.i);
      return true;
    }
    if (v.kind == ValueKind::Float) {
      *out = Value::Float(-v.f);
      return true;
    }
    err->message = "unary '-' requires a number";
    return false;
  }

  static bool ApplyAddSub(const Value& a, const Value& b, bool add, Value* out, EvalError* err) {
    if (!a.is_number() || !b.is_number()) {
      err->message = add ? "addition requires numbers" : "subtraction requires numbers";
      return false;
    }
    if (a.kind == ValueKind::Int && b.kind == ValueKind::Int) {
      std::int64_t r = 0;
      if (add) {
        if (!CheckedAdd(a.i, b.i, &r, err)) return false;
      } else {
        if (!CheckedSub(a.i, b.i, &r, err)) return false;
      }
      *out = Value::Int(r);
      return true;
    }
    const double x = a.as_float();
    const double y = b.as_float();
    *out = Value::Float(add ? x + y : x - y);
    return true;
  }

  static bool ApplyMul(const Value& a, const Value& b, Value* out, EvalError* err) {
    if (!a.is_number() || !b.is_number()) {
      err->message = "multiplication requires numbers";
      return false;
    }
    if (a.kind == ValueKind::Int && b.kind == ValueKind::Int) {
      std::int64_t r = 0;
      if (!CheckedMul(a.i, b.i, &r, err)) return false;
      *out = Value::Int(r);
      return true;
    }
    *out = Value::Float(a.as_float() * b.as_float());
    return true;
  }

  static bool ApplyDiv(const Value& a, const Value& b, Value* out, EvalError* err) {
    if (!a.is_number() || !b.is_number()) {
      err->message = "division requires numbers";
      return false;
    }
    const double y = b.as_float();
    if (y == 0.0) {
      err->message = "division by zero";
      return false;
    }
    // Python 3 true division always yields float.
    *out = Value::Float(a.as_float() / y);
    return true;
  }

  // Python-style remainder (sign follows divisor), same precedence as * /.
  static bool ApplyMod(const Value& a, const Value& b, Value* out, EvalError* err) {
    if (!a.is_number() || !b.is_number()) {
      err->message = "modulo requires numbers";
      return false;
    }
    const double y = b.as_float();
    if (y == 0.0) {
      err->message = "modulo by zero";
      return false;
    }
    if (a.kind == ValueKind::Int && b.kind == ValueKind::Int) {
      // C++ % truncates toward zero; Python uses floor-mod. Match Python.
      const std::int64_t x = a.i;
      const std::int64_t m = b.i;
      std::int64_t r = x % m;
      if (r != 0 && ((r < 0) != (m < 0))) r += m;
      *out = Value::Int(r);
      return true;
    }
    const double x = a.as_float();
    double r = std::fmod(x, y);
    if (r != 0.0 && ((r < 0) != (y < 0))) r += y;
    *out = Value::Float(r);
    return true;
  }

  static bool ApplyPow(const Value& a, const Value& b, Value* out, EvalError* err) {
    if (!a.is_number() || !b.is_number()) {
      err->message = "power requires numbers";
      return false;
    }

    // Integer power when both int and exponent >= 0 (Python keeps int).
    if (a.kind == ValueKind::Int && b.kind == ValueKind::Int && b.i >= 0) {
      if (b.i > 62 && std::llabs(a.i) > 1) {
        err->message = "integer overflow in power";
        return false;
      }
      std::int64_t base = a.i;
      std::int64_t exp = b.i;
      std::int64_t result = 1;
      while (exp > 0) {
        if (exp & 1) {
          if (!CheckedMul(result, base, &result, err)) return false;
        }
        exp >>= 1;
        if (exp) {
          if (!CheckedMul(base, base, &base, err)) return false;
        }
      }
      *out = Value::Int(result);
      return true;
    }

    const double x = a.as_float();
    const double y = b.as_float();
    if (x == 0.0 && y < 0.0) {
      err->message = "0 cannot be raised to a negative power";
      return false;
    }
    const double r = std::pow(x, y);
    if (!std::isfinite(r)) {
      err->message = "power domain/overflow error";
      return false;
    }
    *out = Value::Float(r);
    return true;
  }
};

}  // namespace

ParseResult EvaluateTokens(
    const std::vector<Token>& tokens,
    std::unordered_map<std::string, Value>* env) {
  Parser p(tokens, env);
  return p.ParseStatement();
}

ParseResult EvaluateSource(const std::string& src, std::unordered_map<std::string, Value>* env) {
  ParseResult out;
  auto tok = Tokenize(src);
  if (!tok.ok()) {
    out.error.message = tok.error;
    return out;
  }
  return EvaluateTokens(tok.tokens, env);
}

}  // namespace calc::eval
