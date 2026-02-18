#pragma once
#include "../parser/parser.h"
#include "../semantic/cimple_var.h"
#include "../semantic/type_infer.h"
#include <optional>
#include <string>
#include <unordered_map>


namespace cimple {
namespace eval {

// Legacy Value struct - kept for backward compatibility
// New code should use semantic::CimpleVar for RAII semantics
struct Value {
  enum Kind { Unknown, Int, Float, String, Bool } kind = Unknown;
  long long i = 0;
  double f = 0.0;
  std::string s;
  bool b = false;

  std::string to_string() const;

  // Conversion helpers to/from CimpleVar
  static Value from_cimple_var(const semantic::CimpleVar &var);
  semantic::CimpleVar to_cimple_var() const;
};

// environment mapping variable name -> value
// Using CimpleVar for RAII semantics - automatic cleanup on reassignment
using ValueEnv = std::unordered_map<std::string, semantic::CimpleVar>;

// evaluate an expression given a type env, value env, and function table;
// returns optional value
std::optional<Value> evaluate_expr(
    const parser::Expr *expr, const semantic::TypeEnv &tenv, ValueEnv &venv,
    const std::unordered_map<std::string, parser::FuncDef *> &functions);

// evaluate a statement; returns optional<Value> when a `return` is executed in
// the statement
std::optional<Value> evaluate_stmt(
    const parser::Stmt *stmt, const semantic::TypeEnv &tenv, ValueEnv &venv,
    const std::unordered_map<std::string, parser::FuncDef *> &functions);

} // namespace eval
} // namespace cimple
