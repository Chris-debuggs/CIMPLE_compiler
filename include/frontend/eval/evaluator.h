#pragma once
#include "../parser/parser.h"
#include "../semantic/cimple_var.h"
#include "../semantic/scope_stack.h"
#include "../semantic/type_infer.h"
#include <optional>
#include <string>
#include <unordered_map>

namespace cimple {
namespace eval {

// Runtime value produced by expression evaluation.
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

// Scoped runtime environment.
using ValueEnv = semantic::ScopeStack<semantic::CimpleVar>;

// ---------------------------------------------------------------------------
// StmtResult: structured control-flow signal from statement evaluation.
//
// Every statement returns one of four outcomes:
//   Normal   - execution continues normally
//   Return   - a `return` was hit; carries the returned value (or nullopt)
//   Break    - a `break` was hit inside a loop
//   Continue - a `continue` was hit inside a loop
//
// The while-loop evaluator catches Break and Continue.
// Everything else propagates them upward unchanged (like Return).
// ---------------------------------------------------------------------------
struct StmtResult {
  enum Kind { Normal, Return, Break, Continue } kind = Normal;
  std::optional<Value> value; // populated only for Return

  // Factories for clarity at call sites
  static StmtResult normal() { return {Normal, std::nullopt}; }
  static StmtResult ret(std::optional<Value> v = {}) {
    return {Return, std::move(v)};
  }
  static StmtResult brk() { return {Break, std::nullopt}; }
  static StmtResult cont() { return {Continue, std::nullopt}; }

  bool is_normal() const { return kind == Normal; }
  bool is_return() const { return kind == Return; }
  bool is_break() const { return kind == Break; }
  bool is_continue() const { return kind == Continue; }
};

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// Evaluate an expression. Returns nullopt on evaluation error.
std::optional<Value> evaluate_expr(
    const parser::Expr *expr, const semantic::TypeEnv &tenv, ValueEnv &venv,
    const std::unordered_map<std::string, parser::FuncDef *> &functions);

// Evaluate a statement. Returns a StmtResult signal.
// Callers must propagate non-Normal results upward unless they handle them
// (only while-loops handle Break and Continue).
StmtResult evaluate_stmt(
    const parser::Stmt *stmt, const semantic::TypeEnv &tenv, ValueEnv &venv,
    const std::unordered_map<std::string, parser::FuncDef *> &functions);

} // namespace eval
} // namespace cimple
