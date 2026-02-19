#include "frontend/semantic/type_infer.h"
#include "frontend/semantic/scope_stack.h"
#include <vector>

using namespace cimple;
using namespace cimple::semantic;

namespace {

using TypeScope = cimple::semantic::ScopeStack<cimple::semantic::TypeKind>;

static bool is_numeric(TypeKind t) {
  return t == TypeKind::Int || t == TypeKind::Float;
}

static bool is_comparison_op(const std::string &op) {
  return op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" ||
         op == ">=";
}

static TypeKind unify(TypeKind a, TypeKind b) {
  if (a == TypeKind::Unknown)
    return b;
  if (b == TypeKind::Unknown)
    return a;
  if (a == TypeKind::Void)
    return b;
  if (b == TypeKind::Void)
    return a;
  if (a == b)
    return a;
  if ((a == TypeKind::Int && b == TypeKind::Float) ||
      (a == TypeKind::Float && b == TypeKind::Int)) {
    return TypeKind::Float;
  }
  return TypeKind::Unknown;
}

static TypeKind infer_expr(
    const parser::Expr *e, TypeScope &vars,
    const std::unordered_map<std::string, TypeKind> &functions) {
  if (!e)
    return TypeKind::Unknown;

  if (auto n = dynamic_cast<const parser::NumberLiteral *>(e)) {
    return (n->value.find('.') != std::string::npos) ? TypeKind::Float
                                                      : TypeKind::Int;
  }

  if (dynamic_cast<const parser::StringLiteral *>(e))
    return TypeKind::String;

  if (dynamic_cast<const parser::BoolLiteral *>(e))
    return TypeKind::Bool;

  if (auto v = dynamic_cast<const parser::VarRef *>(e)) {
    if (const auto *found = vars.lookup(v->name))
      return *found;
    return TypeKind::Unknown;
  }

  if (auto u = dynamic_cast<const parser::UnaryOp *>(e)) {
    TypeKind operand = infer_expr(u->operand.get(), vars, functions);
    if (u->op == "not")
      return TypeKind::Bool;
    if (u->op == "-" && is_numeric(operand))
      return operand;
    return TypeKind::Unknown;
  }

  if (auto lg = dynamic_cast<const parser::LogicalExpr *>(e)) {
    infer_expr(lg->left.get(), vars, functions);
    infer_expr(lg->right.get(), vars, functions);
    return TypeKind::Bool;
  }

  if (auto b = dynamic_cast<const parser::BinaryOp *>(e)) {
    TypeKind left = infer_expr(b->left.get(), vars, functions);
    TypeKind right = infer_expr(b->right.get(), vars, functions);

    if (is_comparison_op(b->op)) {
      return TypeKind::Bool;
    }

    if (b->op == "+" && left == TypeKind::String && right == TypeKind::String) {
      return TypeKind::String;
    }

    if (is_numeric(left) && is_numeric(right)) {
      if (b->op == "/") {
        return TypeKind::Float;
      }
      return unify(left, right);
    }

    return TypeKind::Unknown;
  }

  if (auto c = dynamic_cast<const parser::CallExpr *>(e)) {
    if (auto vr = dynamic_cast<const parser::VarRef *>(c->callee.get())) {
      if (vr->name == "print") {
        for (const auto &arg : c->args) {
          infer_expr(arg.get(), vars, functions);
        }
        return TypeKind::Void;
      }
      auto it = functions.find(vr->name);
      if (it != functions.end())
        return it->second;
    }

    for (const auto &arg : c->args) {
      infer_expr(arg.get(), vars, functions);
    }
    return TypeKind::Unknown;
  }

  return TypeKind::Unknown;
}

static TypeKind infer_stmt(
    const parser::Stmt *stmt, TypeScope &vars,
    std::unordered_map<std::string, TypeKind> &functions);

static TypeKind infer_block(
    const std::vector<std::unique_ptr<parser::Stmt>> &body, TypeScope &vars,
    std::unordered_map<std::string, TypeKind> &functions) {
  TypeKind ret = TypeKind::Void;
  for (const auto &stmt : body) {
    if (!stmt)
      continue;
    ret = unify(ret, infer_stmt(stmt.get(), vars, functions));
  }
  return ret;
}

static TypeKind infer_stmt(
    const parser::Stmt *stmt, TypeScope &vars,
    std::unordered_map<std::string, TypeKind> &functions) {
  if (!stmt)
    return TypeKind::Void;

  if (auto a = dynamic_cast<const parser::AssignStmt *>(stmt)) {
    TypeKind rhs = infer_expr(a->value.get(), vars, functions);
    if (auto *current = vars.lookup_current_mut(a->target)) {
      *current = unify(*current, rhs);
    } else {
      vars.set_local(a->target, rhs);
    }
    return TypeKind::Void;
  }

  if (auto es = dynamic_cast<const parser::ExprStmt *>(stmt)) {
    infer_expr(es->expr.get(), vars, functions);
    return TypeKind::Void;
  }

  if (auto rs = dynamic_cast<const parser::ReturnStmt *>(stmt)) {
    return infer_expr(rs->value.get(), vars, functions);
  }

  if (dynamic_cast<const parser::BreakStmt *>(stmt) ||
      dynamic_cast<const parser::ContinueStmt *>(stmt)) {
    return TypeKind::Void;
  }

  if (auto is = dynamic_cast<const parser::IfStmt *>(stmt)) {
    TypeKind branches_ret = TypeKind::Void;

    for (const auto &branch : is->branches) {
      if (branch.condition) {
        infer_expr(branch.condition.get(), vars, functions);
      }

      vars.push_scope(TypeScope::ScopeKind::Block);
      TypeKind body_ret = infer_block(branch.body, vars, functions);
      vars.pop_scope();

      branches_ret = unify(branches_ret, body_ret);
    }

    return branches_ret;
  }

  if (auto ws = dynamic_cast<const parser::WhileStmt *>(stmt)) {
    infer_expr(ws->condition.get(), vars, functions);

    vars.push_scope(TypeScope::ScopeKind::Block);
    TypeKind body_ret = infer_block(ws->body, vars, functions);
    vars.pop_scope();

    return body_ret;
  }

  // Function definitions are inferred in a dedicated pass.
  if (dynamic_cast<const parser::FuncDef *>(stmt)) {
    return TypeKind::Void;
  }

  return TypeKind::Void;
}

static TypeKind infer_function_return(
    const parser::FuncDef *fn, const std::unordered_map<std::string, TypeKind> &global_vars,
    std::unordered_map<std::string, TypeKind> &functions) {
  TypeScope local;
  for (const auto &kv : global_vars) {
    local.set_global(kv.first, kv.second);
  }

  local.push_scope(TypeScope::ScopeKind::Function);
  for (const auto &param : fn->params) {
    local.set_local(param, TypeKind::Unknown);
  }

  TypeKind ret = infer_block(fn->body, local, functions);
  local.pop_scope();
  return ret;
}

static void infer_global_statements(
    const parser::Module &module, TypeScope &globals,
    std::unordered_map<std::string, TypeKind> &functions) {
  for (const auto &stmt : module.body) {
    if (!stmt)
      continue;
    if (dynamic_cast<const parser::FuncDef *>(stmt.get()))
      continue;
    infer_stmt(stmt.get(), globals, functions);
  }
}

} // namespace

TypeEnv cimple::semantic::infer_types(const parser::Module &module) {
  TypeEnv env;

  std::vector<const parser::FuncDef *> function_defs;
  for (const auto &stmt : module.body) {
    if (!stmt)
      continue;
    if (auto fn = dynamic_cast<const parser::FuncDef *>(stmt.get())) {
      env.functions[fn->name] = TypeKind::Unknown;
      function_defs.push_back(fn);
    }
  }

  TypeScope globals;
  infer_global_statements(module, globals, env.functions);
  env.vars = globals.global_values();

  bool changed = true;
  std::size_t iterations = 0;
  const std::size_t max_iterations = function_defs.size() + 2;
  while (changed && iterations < max_iterations) {
    changed = false;
    ++iterations;

    for (const parser::FuncDef *fn : function_defs) {
      TypeKind inferred = infer_function_return(fn, env.vars, env.functions);
      TypeKind &slot = env.functions[fn->name];
      TypeKind merged = unify(slot, inferred);
      if (merged != slot) {
        slot = merged;
        changed = true;
      }
    }
  }

  globals = TypeScope();
  infer_global_statements(module, globals, env.functions);
  env.vars = globals.global_values();

  return env;
}

std::string cimple::semantic::type_to_string(TypeKind t) {
  switch (t) {
  case TypeKind::Unknown:
    return "Unknown";
  case TypeKind::Int:
    return "int";
  case TypeKind::Float:
    return "float";
  case TypeKind::String:
    return "string";
  case TypeKind::Bool:
    return "bool";
  case TypeKind::Void:
    return "void";
  }
  return "?";
}
