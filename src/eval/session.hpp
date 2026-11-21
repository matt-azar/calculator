#pragma once

#include "eval/value.hpp"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace calc::eval {

// In-process calculator session: variables, _, and input history.
// Construct only on first Enter — keep off the cold-launch path.
class Session {
 public:
  Session();

  // Evaluate one line. On success updates '_' and returns formatted result.
  // On failure returns false and fills error_out (does not clear '_').
  bool EvalLine(const std::string& line, std::string* result_out, std::string* error_out);

  void PushHistory(const std::string& line);
  // Up/Down navigation. Returns true and fills line if history moves.
  bool HistoryUp(std::string* line);
  bool HistoryDown(std::string* line);
  void ResetHistoryCursor();

  const std::unordered_map<std::string, Value>& vars() const { return vars_; }

 private:
  std::unordered_map<std::string, Value> vars_;
  std::vector<std::string> history_;
  // history_cursor_ == history_.size() means "editing new line".
  std::size_t history_cursor_ = 0;
};

}  // namespace calc::eval
