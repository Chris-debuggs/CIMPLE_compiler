#pragma once

#include "../parser/parser.h"
#include "scope_stack.h"
#include "type_infer.h"
#include <stdexcept>
#include <string>
#include <vector>

namespace cimple {
namespace semantic {

// Type checking error - thrown when type errors are detected
class TypeCheckError : public std::runtime_error {
public:
  lexer::SourceLocation location;

  TypeCheckError(const std::string &msg, lexer::SourceLocation loc = {0, 0})
      : std::runtime_error(msg), location(loc) {}
};

// Type checker - validates operations against inferred types
// Catches compile-time errors like: string + int, wrong function argument
// types, etc.
class TypeChecker {
public:
  TypeChecker(const parser::Module &module, const TypeEnv &type_env)
      : module_(module), type_env_(type_env) {}

  // Run type checking - throws TypeCheckError if errors found
  void check();

  // Get list of errors (non-throwing version)
  std::vector<std::string> get_errors();

private:
  using ScopedTypeEnv = ScopeStack<TypeKind>;

  const parser::Module &module_;
  const TypeEnv &type_env_;
  std::vector<std::string> errors_;

  void check_stmt(const parser::Stmt *stmt, ScopedTypeEnv &local_env,
                  bool in_loop);

  TypeKind check_expr(const parser::Expr *expr, ScopedTypeEnv &local_env);

  void check_binary_op(const parser::BinaryOp *op, TypeKind left_type,
                       TypeKind right_type, lexer::SourceLocation loc);

  void check_call(const parser::CallExpr *call, ScopedTypeEnv &local_env);

  void check_assignment(const parser::AssignStmt *assign,
                        ScopedTypeEnv &local_env);

  lexer::SourceLocation get_location(const parser::Node *node);

  void add_error(const std::string &msg,
                 lexer::SourceLocation loc = lexer::SourceLocation{0, 0});
};

// Convenience function to check a module
// Returns true if type checking passes, false otherwise
// Errors are collected and can be retrieved
bool check_types(const parser::Module &module, const TypeEnv &type_env,
                 std::vector<std::string> &errors);

} // namespace semantic
} // namespace cimple
