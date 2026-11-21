#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace calc::eval {

enum class TokenKind {
  End,
  Number,
  Name,
  Plus,
  Minus,
  Star,
  Slash,
  Percent,
  StarStar,
  Bang,
  LParen,
  RParen,
  LBracket,
  RBracket,
  Comma,
  Equal,
};

struct Token {
  TokenKind kind = TokenKind::End;
  std::string text;
  double number = 0;
  bool number_is_float = false;
  std::size_t pos = 0;
};

struct TokenizeResult {
  std::vector<Token> tokens;
  std::string error;
  bool ok() const { return error.empty(); }
};

TokenizeResult Tokenize(const std::string& src);

}  // namespace calc::eval
