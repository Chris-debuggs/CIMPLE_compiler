#include "frontend/eval/evaluator.h"
#include <iostream>

using namespace cimple;
using namespace cimple::eval;

std::string Value::to_string() const {
    switch (kind) {
        case Int: return std::to_string(i);
        case Float: return std::to_string(f);
        case String: return s;
        default: return "<unknown>";
    }
}

// Convert CimpleVar to legacy Value struct
Value Value::from_cimple_var(const semantic::CimpleVar& var) {
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

// Convert legacy Value to CimpleVar (RAII-enabled)
semantic::CimpleVar Value::to_cimple_var() const {
    switch (kind) {
        case Int: return semantic::CimpleVar(static_cast<std::int64_t>(i));
        case Float: return semantic::CimpleVar(f);
        case String: return semantic::CimpleVar(s);
        default: return semantic::CimpleVar(std::int64_t(0));
    }
}

static Value make_int(long long v) { Value x; x.kind = Value::Int; x.i = v; return x; }
static Value make_float(double v) { Value x; x.kind = Value::Float; x.f = v; return x; }
static Value make_string(const std::string& s) { Value x; x.kind = Value::String; x.s = s; return x; }

std::optional<Value> cimple::eval::evaluate_expr(const parser::Expr* expr, const semantic::TypeEnv& tenv, ValueEnv& venv,
                                                 const std::unordered_map<std::string, parser::FuncDef*>& functions) {
    if (!expr) return std::nullopt;
    if (auto n = dynamic_cast<const parser::NumberLiteral*>(expr)) {
        if (n->value.find('.') != std::string::npos) {
            semantic::CimpleVar var(std::stod(n->value));
            return Value::from_cimple_var(var);
        }
        semantic::CimpleVar var(static_cast<std::int64_t>(std::stoll(n->value)));
        return Value::from_cimple_var(var);
    }
    if (auto s = dynamic_cast<const parser::StringLiteral*>(expr)) {
        semantic::CimpleVar var(s->value);
        return Value::from_cimple_var(var);
    }
    if (auto v = dynamic_cast<const parser::VarRef*>(expr)) {
        auto it = venv.find(v->name);
        if (it != venv.end()) return Value::from_cimple_var(it->second);
        return std::nullopt;
    }
    if (auto b = dynamic_cast<const parser::BinaryOp*>(expr)) {
        auto L = evaluate_expr(b->left.get(), tenv, venv, functions);
        auto R = evaluate_expr(b->right.get(), tenv, venv, functions);
        if (!L || !R) return std::nullopt;
        
        // Convert to CimpleVar for operations (RAII semantics)
        auto lvar = L->to_cimple_var();
        auto rvar = R->to_cimple_var();
        
        // numeric ops
        if ((lvar.is_int() || lvar.is_float()) && (rvar.is_int() || rvar.is_float())) {
            double lv = lvar.get_float();
            double rv = rvar.get_float();
            semantic::CimpleVar result(0.0);
            if (b->op == "+") result = semantic::CimpleVar(lv + rv);
            else if (b->op == "-") result = semantic::CimpleVar(lv - rv);
            else if (b->op == "*") result = semantic::CimpleVar(lv * rv);
            else if (b->op == "/") result = semantic::CimpleVar(lv / rv);
            else return std::nullopt;
            return Value::from_cimple_var(result);
        }
        // string concatenation
        if (b->op == "+" && lvar.is_string() && rvar.is_string()) {
            std::string concat = lvar.get_string() + rvar.get_string();
            semantic::CimpleVar result(concat);
            return Value::from_cimple_var(result);
        }
        return std::nullopt;
    }
    if (auto c = dynamic_cast<const parser::CallExpr*>(expr)) {
        if (auto callee = dynamic_cast<const parser::VarRef*>(c->callee.get())) {
            // builtin: print
            if (callee->name == "print") {
                for (auto &arg : c->args) {
                    auto v = evaluate_expr(arg.get(), tenv, venv, functions);
                    if (v) std::cout << v->to_string();
                }
                std::cout << std::endl;
                return std::nullopt;
            }

            // user-defined function
            auto it = functions.find(callee->name);
            if (it != functions.end() && it->second) {
                parser::FuncDef* fn = it->second;
                // prepare local env and bind parameters (using CimpleVar for RAII)
                ValueEnv local_venv;
                size_t nparams = fn->params.size();
                for (size_t i = 0; i < nparams && i < c->args.size(); ++i) {
                    auto aval = evaluate_expr(c->args[i].get(), tenv, venv, functions);
                    if (aval) {
                        // Convert Value to CimpleVar - RAII ensures proper cleanup
                        local_venv[fn->params[i]] = aval->to_cimple_var();
                    }
                }

                // create local type env (simple, reuse tenv for globals)
                semantic::TypeEnv local_tenv;

                // execute function body statements
                for (auto &bs : fn->body) {
                    auto ret = cimple::eval::evaluate_stmt(bs.get(), local_tenv, local_venv, functions);
                    if (ret) return ret; // propagate return value
                }
                return std::nullopt;
            }
        }
    }
    return std::nullopt;
}

std::optional<Value> cimple::eval::evaluate_stmt(const parser::Stmt* stmt, const semantic::TypeEnv& tenv, ValueEnv& venv,
                                                 const std::unordered_map<std::string, parser::FuncDef*>& functions) {
    if (!stmt) return std::nullopt;
    if (auto as = dynamic_cast<const parser::AssignStmt*>(stmt)) {
        auto v = evaluate_expr(as->value.get(), tenv, venv, functions);
        if (v) {
            // assign into venv using CimpleVar - RAII ensures old value is cleaned up automatically
            venv[as->target] = v->to_cimple_var();
        }
        return std::nullopt;
    }
    if (auto es = dynamic_cast<const parser::ExprStmt*>(stmt)) {
        evaluate_expr(es->expr.get(), tenv, venv, functions);
        return std::nullopt;
    }
    if (auto rs = dynamic_cast<const parser::ReturnStmt*>(stmt)) {
        auto v = evaluate_expr(rs->value.get(), tenv, venv, functions);
        return v;
    }
    return std::nullopt;
}
