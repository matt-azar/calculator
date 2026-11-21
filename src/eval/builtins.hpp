#pragma once

#include "eval/value.hpp"

#include <string>
#include <vector>

namespace calc::eval {

// Call a named builtin. Returns false and fills err on failure.
bool CallBuiltin(const std::string& name, const std::vector<Value>& args, Value* out, EvalError* err);

bool IsBuiltinName(const std::string& name);

}  // namespace calc::eval
