#include "eval/tokenize.hpp"

#include <cctype>

namespace calc::eval {
namespace {

bool IsIdentStart(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool IsIdentCont(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

}  // namespace

TokenizeResult Tokenize(const std::string& src) {
  TokenizeResult out;
  std::size_t i = 0;
  const std::size_t n = src.size();

  auto push = [&](TokenKind kind, std::size_t pos, std::string text = {}) {
    Token t;
    t.kind = kind;
    t.pos = pos;
    t.text = std::move(text);
    out.tokens.push_back(std::move(t));
  };

  while (i < n) {
    const char c = src[i];
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      ++i;
      continue;
    }

    if (std::isdigit(static_cast<unsigned char>(c)) ||
        (c == '.' && i + 1 < n && std::isdigit(static_cast<unsigned char>(src[i + 1])))) {
      const std::size_t start = i;
      bool is_float = false;
      while (i < n && std::isdigit(static_cast<unsigned char>(src[i]))) ++i;
      if (i < n && src[i] == '.') {
        is_float = true;
        ++i;
        while (i < n && std::isdigit(static_cast<unsigned char>(src[i]))) ++i;
      }
      if (i < n && (src[i] == 'e' || src[i] == 'E')) {
        is_float = true;
        ++i;
        if (i < n && (src[i] == '+' || src[i] == '-')) ++i;
        if (i >= n || !std::isdigit(static_cast<unsigned char>(src[i]))) {
          out.error = "malformed exponent in number";
          return out;
        }
        while (i < n && std::isdigit(static_cast<unsigned char>(src[i]))) ++i;
      }
      Token t;
      t.kind = TokenKind::Number;
      t.pos = start;
      t.text = src.substr(start, i - start);
      t.number_is_float = is_float || t.text.find('.') != std::string::npos;
      try {
        if (t.number_is_float) {
          t.number = std::stod(t.text);
        } else {
          // Keep as float payload too for convenience; parser prefers int path.
          const long long v = std::stoll(t.text);
          t.number = static_cast<double>(v);
        }
      } catch (...) {
        out.error = "invalid number literal";
        return out;
      }
      out.tokens.push_back(std::move(t));
      continue;
    }

    if (IsIdentStart(c)) {
      const std::size_t start = i;
      ++i;
      while (i < n && IsIdentCont(src[i])) ++i;
      push(TokenKind::Name, start, src.substr(start, i - start));
      continue;
    }

    if (c == '*' && i + 1 < n && src[i + 1] == '*') {
      push(TokenKind::StarStar, i, "**");
      i += 2;
      continue;
    }

    switch (c) {
      case '+': push(TokenKind::Plus, i, "+"); break;
      case '-': push(TokenKind::Minus, i, "-"); break;
      case '*': push(TokenKind::Star, i, "*"); break;
      case '/': push(TokenKind::Slash, i, "/"); break;
      case '%': push(TokenKind::Percent, i, "%"); break;
      case '!': push(TokenKind::Bang, i, "!"); break;
      case '(': push(TokenKind::LParen, i, "("); break;
      case ')': push(TokenKind::RParen, i, ")"); break;
      case '[': push(TokenKind::LBracket, i, "["); break;
      case ']': push(TokenKind::RBracket, i, "]"); break;
      case ',': push(TokenKind::Comma, i, ","); break;
      case '=': push(TokenKind::Equal, i, "="); break;
      default:
        out.error = std::string("unexpected character '") + c + "'";
        return out;
    }
    ++i;
  }

  push(TokenKind::End, n);
  return out;
}

}  // namespace calc::eval
