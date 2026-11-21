#pragma once

#include "eval/tokenize.hpp"
#include "eval/value.hpp"

#include <string>
#include <unordered_map>

namespace calc::eval {

struct ParseResult {
  Value value;
  EvalError error;
  bool ok = false;
};

// Evaluate a tokenized expression with variable lookup/store callbacks via env.
// Assignment form: NAME '=' expr (statement-level).
ParseResult EvaluateTokens(
    const std::vector<Token>& tokens,
    std::unordered_map<std::string, Value>* env);

ParseResult EvaluateSource(const std::string& src, std::unordered_map<std::string, Value>* env);

}  // namespace calc::eval
