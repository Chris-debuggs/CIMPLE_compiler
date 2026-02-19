// type_checker.cpp - Static type checking implementation
#include "frontend/semantic/type_checker.h"
#include <sstream>

using namespace cimple;
using namespace cimple::semantic;

namespace {

static bool is_numeric(TypeKind t) {
  return t == TypeKind::Int || t == TypeKind::Float;
}

static bool is_truthy_compatible(TypeKind t) {
  return t == TypeKind::Unknown || t == TypeKind::Bool || t == TypeKind::Int ||
         t == TypeKind::Float || t == TypeKind::String;
}

static bool is_comparison_op(const std::string &op) {
  return op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" ||
         op == ">=";
}

static TypeKind merge_assignment_type(TypeKind existing, TypeKind incoming) {
  if (existing == TypeKind::Unknown)
    return incoming;
  if (incoming == TypeKind::Unknown)
    return existing;
  if ((existing == TypeKind::Int && incoming == TypeKind::Float) ||
      (existing == TypeKind::Float && incoming == TypeKind::Int)) {
    return TypeKind::Float;
  }
  return incoming;
}

} // namespace

void TypeChecker::add_error(const std::string &msg, lexer::SourceLocation loc) {
  if (loc.line > 0 && loc.column > 0) {
    std::ostringstream os;
    os << msg << " (at " << loc.line << ":" << loc.column << ")";
    errors_.push_back(os.str());
  } else {
    errors_.push_back(msg);
  }
}

void TypeChecker::check() {
  errors_ = get_errors();

  if (!errors_.empty()) {
    std::ostringstream msg;
    msg << "Type checking failed with " << errors_.size() << " error(s)";
    throw TypeCheckError(msg.str());
  }
}

std::vector<std::string> TypeChecker::get_errors() {
  errors_.clear();

  ScopedTypeEnv env;
  for (const auto &kv : type_env_.vars) {
    env.set_global(kv.first, kv.second);
  }

  for (const auto &stmt : module_.body) {
    if (stmt) {
      check_stmt(stmt.get(), env, false);
    }
  }

  return errors_;
}

void TypeChecker::check_stmt(const parser::Stmt *stmt, ScopedTypeEnv &local_env,
                             bool in_loop) {
  if (!stmt)
    return;

  if (auto assign = dynamic_cast<const parser::AssignStmt *>(stmt)) {
    check_assignment(assign, local_env);
    return;
  }

  if (auto expr_stmt = dynamic_cast<const parser::ExprStmt *>(stmt)) {
    check_expr(expr_stmt->expr.get(), local_env);
    return;
  }

  if (auto ret = dynamic_cast<const parser::ReturnStmt *>(stmt)) {
    check_expr(ret->value.get(), local_env);
    return;
  }

  if (dynamic_cast<const parser::BreakStmt *>(stmt)) {
    if (!in_loop) {
      add_error("'break' used outside of loop", get_location(stmt));
    }
    return;
  }

  if (dynamic_cast<const parser::ContinueStmt *>(stmt)) {
    if (!in_loop) {
      add_error("'continue' used outside of loop", get_location(stmt));
    }
    return;
  }

  if (auto func_def = dynamic_cast<const parser::FuncDef *>(stmt)) {
    local_env.push_scope(ScopedTypeEnv::ScopeKind::Function);

    for (const auto &param : func_def->params) {
      local_env.set_local(param, TypeKind::Unknown);
    }

    for (const auto &body_stmt : func_def->body) {
      if (body_stmt) {
        check_stmt(body_stmt.get(), local_env, false);
      }
    }

    local_env.pop_scope();
    return;
  }

  if (auto if_stmt = dynamic_cast<const parser::IfStmt *>(stmt)) {
    for (const auto &branch : if_stmt->branches) {
      if (branch.condition) {
        TypeKind cond = check_expr(branch.condition.get(), local_env);
        if (!is_truthy_compatible(cond)) {
          add_error("if-condition is not truthy-compatible",
                    get_location(branch.condition.get()));
        }
      }

      local_env.push_scope(ScopedTypeEnv::ScopeKind::Block);
      for (const auto &body_stmt : branch.body) {
        if (body_stmt) {
          check_stmt(body_stmt.get(), local_env, in_loop);
        }
      }
      local_env.pop_scope();
    }
    return;
  }

  if (auto while_stmt = dynamic_cast<const parser::WhileStmt *>(stmt)) {
    TypeKind cond = check_expr(while_stmt->condition.get(), local_env);
    if (!is_truthy_compatible(cond)) {
      add_error("while-condition is not truthy-compatible",
                get_location(while_stmt->condition.get()));
    }

    local_env.push_scope(ScopedTypeEnv::ScopeKind::Block);
    for (const auto &body_stmt : while_stmt->body) {
      if (body_stmt) {
        check_stmt(body_stmt.get(), local_env, true);
      }
    }
    local_env.pop_scope();
    return;
  }
}

TypeKind TypeChecker::check_expr(const parser::Expr *expr,
                                 ScopedTypeEnv &local_env) {
  if (!expr)
    return TypeKind::Unknown;

  if (auto num = dynamic_cast<const parser::NumberLiteral *>(expr)) {
    return (num->value.find('.') != std::string::npos) ? TypeKind::Float
                                                        : TypeKind::Int;
  }

  if (dynamic_cast<const parser::StringLiteral *>(expr)) {
    return TypeKind::String;
  }

  if (dynamic_cast<const parser::BoolLiteral *>(expr)) {
    return TypeKind::Bool;
  }

  if (auto var_ref = dynamic_cast<const parser::VarRef *>(expr)) {
    if (const auto *found = local_env.lookup(var_ref->name)) {
      return *found;
    }
    return TypeKind::Unknown;
  }

  if (auto unary = dynamic_cast<const parser::UnaryOp *>(expr)) {
    TypeKind operand = check_expr(unary->operand.get(), local_env);

    if (unary->op == "not") {
      if (!is_truthy_compatible(operand)) {
        add_error("Operand of 'not' must be truthy-compatible",
                  get_location(unary));
      }
      return TypeKind::Bool;
    }

    if (unary->op == "-") {
      if (!is_numeric(operand) && operand != TypeKind::Unknown) {
        add_error("Unary '-' operand must be numeric", get_location(unary));
      }
      return operand;
    }

    return TypeKind::Unknown;
  }

  if (auto logical = dynamic_cast<const parser::LogicalExpr *>(expr)) {
    TypeKind left_type = check_expr(logical->left.get(), local_env);
    TypeKind right_type = check_expr(logical->right.get(), local_env);

    if (!is_truthy_compatible(left_type)) {
      add_error("Left operand of logical operator must be truthy-compatible",
                get_location(logical));
    }
    if (!is_truthy_compatible(right_type)) {
      add_error("Right operand of logical operator must be truthy-compatible",
                get_location(logical));
    }

    return TypeKind::Bool;
  }

  if (auto bin_op = dynamic_cast<const parser::BinaryOp *>(expr)) {
    TypeKind left_type = check_expr(bin_op->left.get(), local_env);
    TypeKind right_type = check_expr(bin_op->right.get(), local_env);

    check_binary_op(bin_op, left_type, right_type, get_location(bin_op));

    if (is_comparison_op(bin_op->op)) {
      return TypeKind::Bool;
    }

    if (bin_op->op == "+" && left_type == TypeKind::String &&
        right_type == TypeKind::String) {
      return TypeKind::String;
    }

    if (is_numeric(left_type) && is_numeric(right_type)) {
      if (bin_op->op == "/") {
        return TypeKind::Float;
      }
      return (left_type == TypeKind::Float || right_type == TypeKind::Float)
                 ? TypeKind::Float
                 : TypeKind::Int;
    }

    return TypeKind::Unknown;
  }

  if (auto call = dynamic_cast<const parser::CallExpr *>(expr)) {
    check_call(call, local_env);

    if (auto callee_var = dynamic_cast<const parser::VarRef *>(call->callee.get())) {
      if (callee_var->name == "print") {
        return TypeKind::Void;
      }

      auto it = type_env_.functions.find(callee_var->name);
      if (it != type_env_.functions.end()) {
        return it->second;
      }
    }

    return TypeKind::Unknown;
  }

  return TypeKind::Unknown;
}

void TypeChecker::check_binary_op(const parser::BinaryOp *op, TypeKind left_type,
                                  TypeKind right_type,
                                  lexer::SourceLocation loc) {
  if (!op)
    return;

  if (is_comparison_op(op->op)) {
    const bool both_numeric = is_numeric(left_type) && is_numeric(right_type);
    const bool both_string =
        left_type == TypeKind::String && right_type == TypeKind::String;
    const bool both_bool = left_type == TypeKind::Bool && right_type == TypeKind::Bool;
    const bool has_unknown =
        left_type == TypeKind::Unknown || right_type == TypeKind::Unknown;

    if (both_numeric || both_string || has_unknown)
      return;

    if (both_bool && (op->op == "==" || op->op == "!="))
      return;

    add_error("Invalid operand types for comparison operator '" + op->op + "'",
              loc);
    return;
  }

  if (op->op == "+" && (left_type == TypeKind::String || right_type == TypeKind::String)) {
    if (!(left_type == TypeKind::String && right_type == TypeKind::String)) {
      add_error("String concatenation requires string + string", loc);
    }
    return;
  }

  if (op->op == "+" || op->op == "-" || op->op == "*" || op->op == "/") {
    if (!is_numeric(left_type) && left_type != TypeKind::Unknown) {
      add_error("Left operand of '" + op->op + "' must be numeric, got " +
                    type_to_string(left_type),
                loc);
    }

    if (!is_numeric(right_type) && right_type != TypeKind::Unknown) {
      add_error("Right operand of '" + op->op + "' must be numeric, got " +
                    type_to_string(right_type),
                loc);
    }
  }
}

void TypeChecker::check_call(const parser::CallExpr *call,
                             ScopedTypeEnv &local_env) {
  if (!call || !call->callee)
    return;

  for (const auto &arg : call->args) {
    check_expr(arg.get(), local_env);
  }

  if (auto callee_var = dynamic_cast<const parser::VarRef *>(call->callee.get())) {
    if (callee_var->name == "print")
      return;

    if (type_env_.functions.find(callee_var->name) == type_env_.functions.end()) {
      add_error("Call to unknown function '" + callee_var->name + "'",
                get_location(call));
    }
  }
}

void TypeChecker::check_assignment(const parser::AssignStmt *assign,
                                   ScopedTypeEnv &local_env) {
  if (!assign)
    return;

  TypeKind value_type = check_expr(assign->value.get(), local_env);

  if (const auto *existing = local_env.lookup_current(assign->target)) {
    if (*existing != TypeKind::Unknown && value_type != TypeKind::Unknown) {
      const bool both_numeric = is_numeric(*existing) && is_numeric(value_type);
      if (!both_numeric && *existing != value_type) {
        add_error("Cannot assign " + type_to_string(value_type) +
                      " to variable '" + assign->target + "' of type " +
                      type_to_string(*existing),
                  get_location(assign));
        return;
      }
    }

    local_env.set_local(assign->target, merge_assignment_type(*existing, value_type));
    return;
  }

  local_env.set_local(assign->target, value_type);
}

lexer::SourceLocation TypeChecker::get_location(const parser::Node *node) {
  (void)node;
  // For now, return default location.
  // In a full implementation, AST nodes would store source locations.
  return {0, 0};
}

namespace cimple {
namespace semantic {

bool check_types(const parser::Module &module, const TypeEnv &type_env,
                 std::vector<std::string> &errors) {
  TypeChecker checker(module, type_env);
  errors = checker.get_errors();
  return errors.empty();
}

} // namespace semantic
} // namespace cimple
