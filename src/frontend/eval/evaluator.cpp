#include "frontend/eval/evaluator.h"
#include <cmath>
#include <iostream>

using namespace cimple;
using namespace cimple::eval;

// ---------------------------------------------------------------------------
// Value helpers
// ---------------------------------------------------------------------------

std::string Value::to_string() const {
  switch (kind) {
  case Int:
    return std::to_string(i);
  case Float:
    return std::to_string(f);
  case String:
    return s;
  case Bool:
    return b ? "True" : "False";
  default:
    return "<unknown>";
  }
}

Value Value::from_cimple_var(const semantic::CimpleVar &var) {
  Value v;
  if (var.is_int()) {
    v.kind = Int;
    v.i = var.get_int();
  } else if (var.is_float()) {
    v.kind = Float;
    v.f = var.get_float();
  } else if (var.is_string()) {
    v.kind = String;
    v.s = var.get_string();
  } else {
    v.kind = Unknown;
  }
  return v;
}

semantic::CimpleVar Value::to_cimple_var() const {
  switch (kind) {
  case Int:
    return semantic::CimpleVar(static_cast<std::int64_t>(i));
  case Float:
    return semantic::CimpleVar(f);
  case String:
    return semantic::CimpleVar(s);
  case Bool:
    return semantic::CimpleVar(static_cast<std::int64_t>(b ? 1 : 0));
  default:
    return semantic::CimpleVar(std::int64_t(0));
  }
}

// Return true if this Value is truthy (for if/while conditions)
static bool is_truthy(const Value &v) {
  switch (v.kind) {
  case Value::Int:
    return v.i != 0;
  case Value::Float:
    return v.f != 0.0;
  case Value::String:
    return !v.s.empty();
  case Value::Bool:
    return v.b;
  default:
    return false;
  }
}

static Value make_int(long long v) {
  Value x;
  x.kind = Value::Int;
  x.i = v;
  return x;
}
static Value make_float(double v) {
  Value x;
  x.kind = Value::Float;
  x.f = v;
  return x;
}
static Value make_string(const std::string &s) {
  Value x;
  x.kind = Value::String;
  x.s = s;
  return x;
}
static Value make_bool(bool v) {
  Value x;
  x.kind = Value::Bool;
  x.b = v;
  return x;
}

// ---------------------------------------------------------------------------
// evaluate_expr
// ---------------------------------------------------------------------------

std::optional<Value> cimple::eval::evaluate_expr(
    const parser::Expr *expr, const semantic::TypeEnv &tenv, ValueEnv &venv,
    const std::unordered_map<std::string, parser::FuncDef *> &functions) {
  if (!expr)
    return std::nullopt;

  // --- Literals ---
  if (auto n = dynamic_cast<const parser::NumberLiteral *>(expr)) {
    if (n->value.find('.') != std::string::npos)
      return make_float(std::stod(n->value));
    return make_int(std::stoll(n->value));
  }
  if (auto s = dynamic_cast<const parser::StringLiteral *>(expr)) {
    // Strip surrounding quotes from the lexer's raw string token
    std::string raw = s->value;
    if (raw.size() >= 2 && (raw.front() == '"' || raw.front() == '\''))
      raw = raw.substr(1, raw.size() - 2);
    return make_string(raw);
  }
  if (auto bl = dynamic_cast<const parser::BoolLiteral *>(expr)) {
    return make_bool(bl->value);
  }

  // --- Variable reference ---
  if (auto v = dynamic_cast<const parser::VarRef *>(expr)) {
    auto it = venv.find(v->name);
    if (it != venv.end())
      return Value::from_cimple_var(it->second);
    return std::nullopt;
  }

  // --- Unary operators ---
  if (auto u = dynamic_cast<const parser::UnaryOp *>(expr)) {
    auto operand = evaluate_expr(u->operand.get(), tenv, venv, functions);
    if (!operand)
      return std::nullopt;
    if (u->op == "not")
      return make_bool(!is_truthy(*operand));
    if (u->op == "-") {
      if (operand->kind == Value::Int)
        return make_int(-operand->i);
      if (operand->kind == Value::Float)
        return make_float(-operand->f);
    }
    return std::nullopt;
  }

  // --- Logical operators (and / or) with short-circuit evaluation ---
  // These are distinct from BinaryOp: the RHS may never be evaluated.
  // Both always return a Bool result (truthiness semantics).
  if (auto lg = dynamic_cast<const parser::LogicalExpr *>(expr)) {
    // Evaluate left operand unconditionally
    auto left = evaluate_expr(lg->left.get(), tenv, venv, functions);
    if (!left)
      return std::nullopt;

    if (lg->op == "and") {
      // Short-circuit: if left is falsy, skip right entirely
      if (!is_truthy(*left))
        return make_bool(false);
      auto right = evaluate_expr(lg->right.get(), tenv, venv, functions);
      if (!right)
        return std::nullopt;
      return make_bool(is_truthy(*right));
    }

    if (lg->op == "or") {
      // Short-circuit: if left is truthy, skip right entirely
      if (is_truthy(*left))
        return make_bool(true);
      auto right = evaluate_expr(lg->right.get(), tenv, venv, functions);
      if (!right)
        return std::nullopt;
      return make_bool(is_truthy(*right));
    }

    return std::nullopt;
  }

  // --- Binary operators ---
  if (auto b = dynamic_cast<const parser::BinaryOp *>(expr)) {
    auto L = evaluate_expr(b->left.get(), tenv, venv, functions);
    auto R = evaluate_expr(b->right.get(), tenv, venv, functions);
    if (!L || !R)
      return std::nullopt;

    // Comparison operators — work on numbers and strings
    auto is_cmp = [](const std::string &op) {
      return op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" ||
             op == ">=";
    };
    if (is_cmp(b->op)) {
      // Numeric comparison
      if ((L->kind == Value::Int || L->kind == Value::Float) &&
          (R->kind == Value::Int || R->kind == Value::Float)) {
        double lv = (L->kind == Value::Int) ? (double)L->i : L->f;
        double rv = (R->kind == Value::Int) ? (double)R->i : R->f;
        if (b->op == "==")
          return make_bool(lv == rv);
        if (b->op == "!=")
          return make_bool(lv != rv);
        if (b->op == "<")
          return make_bool(lv < rv);
        if (b->op == ">")
          return make_bool(lv > rv);
        if (b->op == "<=")
          return make_bool(lv <= rv);
        if (b->op == ">=")
          return make_bool(lv >= rv);
      }
      // String comparison
      if (L->kind == Value::String && R->kind == Value::String) {
        if (b->op == "==")
          return make_bool(L->s == R->s);
        if (b->op == "!=")
          return make_bool(L->s != R->s);
        if (b->op == "<")
          return make_bool(L->s < R->s);
        if (b->op == ">")
          return make_bool(L->s > R->s);
        if (b->op == "<=")
          return make_bool(L->s <= R->s);
        if (b->op == ">=")
          return make_bool(L->s >= R->s);
      }
      // Bool equality
      if (L->kind == Value::Bool && R->kind == Value::Bool) {
        if (b->op == "==")
          return make_bool(L->b == R->b);
        if (b->op == "!=")
          return make_bool(L->b != R->b);
      }
      return std::nullopt;
    }

    // Arithmetic operators
    if ((L->kind == Value::Int || L->kind == Value::Float) &&
        (R->kind == Value::Int || R->kind == Value::Float)) {
      // Preserve int if both operands are int and result is exact
      bool both_int = (L->kind == Value::Int && R->kind == Value::Int);
      if (both_int) {
        long long lv = L->i, rv = R->i;
        if (b->op == "+")
          return make_int(lv + rv);
        if (b->op == "-")
          return make_int(lv - rv);
        if (b->op == "*")
          return make_int(lv * rv);
        if (b->op == "/") {
          if (rv == 0) {
            std::cerr << "Division by zero\n";
            return std::nullopt;
          }
          // Integer division if evenly divisible, else float
          if (lv % rv == 0)
            return make_int(lv / rv);
          return make_float((double)lv / (double)rv);
        }
      } else {
        double lv = (L->kind == Value::Int) ? (double)L->i : L->f;
        double rv = (R->kind == Value::Int) ? (double)R->i : R->f;
        if (b->op == "+")
          return make_float(lv + rv);
        if (b->op == "-")
          return make_float(lv - rv);
        if (b->op == "*")
          return make_float(lv * rv);
        if (b->op == "/") {
          if (rv == 0.0) {
            std::cerr << "Division by zero\n";
            return std::nullopt;
          }
          return make_float(lv / rv);
        }
      }
    }
    // String concatenation
    if (b->op == "+" && L->kind == Value::String && R->kind == Value::String)
      return make_string(L->s + R->s);

    return std::nullopt;
  }

  // --- Function call ---
  if (auto c = dynamic_cast<const parser::CallExpr *>(expr)) {
    if (auto callee = dynamic_cast<const parser::VarRef *>(c->callee.get())) {
      // builtin: print
      if (callee->name == "print") {
        for (auto &arg : c->args) {
          auto v = evaluate_expr(arg.get(), tenv, venv, functions);
          if (v)
            std::cout << v->to_string();
        }
        std::cout << std::endl;
        return std::nullopt;
      }

      // user-defined function
      auto it = functions.find(callee->name);
      if (it != functions.end() && it->second) {
        parser::FuncDef *fn = it->second;
        ValueEnv local_venv;
        size_t nparams = fn->params.size();
        for (size_t i = 0; i < nparams && i < c->args.size(); ++i) {
          auto aval = evaluate_expr(c->args[i].get(), tenv, venv, functions);
          if (aval)
            local_venv[fn->params[i]] = aval->to_cimple_var();
        }
        semantic::TypeEnv local_tenv;
        for (auto &bs : fn->body) {
          auto res = cimple::eval::evaluate_stmt(bs.get(), local_tenv,
                                                 local_venv, functions);
          if (res.is_return())
            return res.value; // unwrap Value from StmtResult::Return
          // Break/Continue can't escape a function call — treat as error/ignore
        }
        return std::nullopt;
      }
    }
  }
  return std::nullopt;
}

// ---------------------------------------------------------------------------
// evaluate_stmt
// ---------------------------------------------------------------------------

StmtResult cimple::eval::evaluate_stmt(
    const parser::Stmt *stmt, const semantic::TypeEnv &tenv, ValueEnv &venv,
    const std::unordered_map<std::string, parser::FuncDef *> &functions) {
  if (!stmt)
    return StmtResult::normal();

  // --- Assignment ---
  if (auto as = dynamic_cast<const parser::AssignStmt *>(stmt)) {
    auto v = evaluate_expr(as->value.get(), tenv, venv, functions);
    if (v)
      venv[as->target] = v->to_cimple_var();
    return StmtResult::normal();
  }

  // --- Expression statement (e.g. a function call like print(...)) ---
  if (auto es = dynamic_cast<const parser::ExprStmt *>(stmt)) {
    evaluate_expr(es->expr.get(), tenv, venv, functions);
    return StmtResult::normal();
  }

  // --- return ---
  if (auto rs = dynamic_cast<const parser::ReturnStmt *>(stmt)) {
    auto v = evaluate_expr(rs->value.get(), tenv, venv, functions);
    return StmtResult::ret(v);
  }

  // --- break ---
  if (dynamic_cast<const parser::BreakStmt *>(stmt))
    return StmtResult::brk();

  // --- continue ---
  if (dynamic_cast<const parser::ContinueStmt *>(stmt))
    return StmtResult::cont();

  // --- FuncDef at statement level (registered by module runner, skip here) ---
  if (dynamic_cast<const parser::FuncDef *>(stmt))
    return StmtResult::normal();

  // --- if / elif / else ---
  if (auto is = dynamic_cast<const parser::IfStmt *>(stmt)) {
    for (auto &branch : is->branches) {
      bool take = false;
      if (!branch.condition) {
        take = true; // else branch
      } else {
        auto cond =
            evaluate_expr(branch.condition.get(), tenv, venv, functions);
        take = cond && is_truthy(*cond);
      }
      if (take) {
        for (auto &s : branch.body) {
          auto res = evaluate_stmt(s.get(), tenv, venv, functions);
          if (!res.is_normal())
            return res; // propagate Return, Break, Continue
        }
        break; // only execute the first matching branch
      }
    }
    return StmtResult::normal();
  }

  // --- while ---
  // This is the ONLY place that catches Break and Continue.
  // Return still propagates upward.
  if (auto ws = dynamic_cast<const parser::WhileStmt *>(stmt)) {
    while (true) {
      // Evaluate condition — exits loop if falsy
      auto cond = evaluate_expr(ws->condition.get(), tenv, venv, functions);
      if (!cond || !is_truthy(*cond))
        break;

      // Execute loop body
      bool did_break = false;
      for (auto &s : ws->body) {
        auto res = evaluate_stmt(s.get(), tenv, venv, functions);
        if (res.is_break()) {
          did_break = true;
          break; // break exits the body loop
        }
        if (res.is_continue()) {
          break; // continue skips rest of body, re-checks condition
        }
        if (res.is_return())
          return res; // propagate return upward (out of while)
      }
      if (did_break)
        break; // break exits the while loop itself
    }
    return StmtResult::normal();
  }

  return StmtResult::normal();
}
