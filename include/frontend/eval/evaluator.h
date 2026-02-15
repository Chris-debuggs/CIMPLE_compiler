#pragma once
#include "../parser/parser.h"
#include "../semantic/type_infer.h"
#include <string>
#include <optional>
#include <unordered_map>

namespace cimple {
namespace eval {

struct Value {
    enum Kind { Unknown, Int, Float, String } kind = Unknown;
    long long i = 0;
    double f = 0.0;
    std::string s;

    std::string to_string() const;
};

// environment mapping variable name -> value
using ValueEnv = std::unordered_map<std::string, Value>;

// evaluate an expression given a type env, value env, and function table; returns optional value
std::optional<Value> evaluate_expr(const parser::Expr* expr, const semantic::TypeEnv& tenv, ValueEnv& venv,
                                    const std::unordered_map<std::string, parser::FuncDef*>& functions);

// evaluate a statement; returns optional<Value> when a `return` is executed in the statement
std::optional<Value> evaluate_stmt(const parser::Stmt* stmt, const semantic::TypeEnv& tenv, ValueEnv& venv,
                                    const std::unordered_map<std::string, parser::FuncDef*>& functions);

} // namespace eval
} // namespace cimple
