#include "eval/session.hpp"

#include "eval/parse.hpp"

namespace calc::eval {

Session::Session() = default;

bool Session::EvalLine(const std::string& line, std::string* result_out, std::string* error_out) {
  auto trimmed = line;
  // Light trim of ends.
  while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t')) trimmed.pop_back();
  std::size_t start = 0;
  while (start < trimmed.size() && (trimmed[start] == ' ' || trimmed[start] == '\t')) ++start;
  trimmed = trimmed.substr(start);

  if (trimmed.empty()) {
    if (error_out) *error_out = "empty expression";
    return false;
  }

  ParseResult pr = EvaluateSource(trimmed, &vars_);
  if (!pr.ok) {
    if (error_out) *error_out = pr.error.message;
    return false;
  }

  vars_["_"] = pr.value;
  if (result_out) *result_out = FormatValue(pr.value);
  PushHistory(trimmed);
  ResetHistoryCursor();
  return true;
}

void Session::PushHistory(const std::string& line) {
  if (!history_.empty() && history_.back() == line) {
    return;
  }
  history_.push_back(line);
}

bool Session::HistoryUp(std::string* line) {
  if (history_.empty()) return false;
  if (history_cursor_ == 0) return false;
  --history_cursor_;
  if (line) *line = history_[history_cursor_];
  return true;
}

bool Session::HistoryDown(std::string* line) {
  if (history_.empty()) return false;
  if (history_cursor_ >= history_.size()) return false;
  ++history_cursor_;
  if (history_cursor_ >= history_.size()) {
    if (line) line->clear();
    return true;
  }
  if (line) *line = history_[history_cursor_];
  return true;
}

void Session::ResetHistoryCursor() {
  history_cursor_ = history_.size();
}

}  // namespace calc::eval
