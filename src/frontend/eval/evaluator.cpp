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

static Value make_int(long long v) { Value x; x.kind = Value::Int; x.i = v; return x; }
static Value make_float(double v) { Value x; x.kind = Value::Float; x.f = v; return x; }
static Value make_string(const std::string& s) { Value x; x.kind = Value::String; x.s = s; return x; }

std::optional<Value> cimple::eval::evaluate_expr(const parser::Expr* expr, const semantic::TypeEnv& tenv, ValueEnv& venv,
                                                 const std::unordered_map<std::string, parser::FuncDef*>& functions) {
    if (!expr) return std::nullopt;
    if (auto n = dynamic_cast<const parser::NumberLiteral*>(expr)) {
        if (n->value.find('.') != std::string::npos) return make_float(std::stod(n->value));
        return make_int(std::stoll(n->value));
    }
    if (auto s = dynamic_cast<const parser::StringLiteral*>(expr)) {
        return make_string(s->value);
    }
    if (auto v = dynamic_cast<const parser::VarRef*>(expr)) {
        auto it = venv.find(v->name);
        if (it != venv.end()) return it->second;
        return std::nullopt;
    }
    if (auto b = dynamic_cast<const parser::BinaryOp*>(expr)) {
        auto L = evaluate_expr(b->left.get(), tenv, venv, functions);
        auto R = evaluate_expr(b->right.get(), tenv, venv, functions);
        if (!L || !R) return std::nullopt;
        // numeric ops
        if ((L->kind == Value::Int || L->kind == Value::Float) && (R->kind == Value::Int || R->kind == Value::Float)) {
            double lv = (L->kind==Value::Int) ? (double)L->i : L->f;
            double rv = (R->kind==Value::Int) ? (double)R->i : R->f;
            if (b->op == "+") return make_float(lv + rv);
            if (b->op == "-") return make_float(lv - rv);
            if (b->op == "*") return make_float(lv * rv);
            if (b->op == "/") return make_float(lv / rv);
        }
        // string concatenation
        if (b->op == "+" && L->kind == Value::String && R->kind == Value::String) {
            return make_string(L->s + R->s);
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
                // prepare local env and bind parameters
                ValueEnv local_venv;
                size_t nparams = fn->params.size();
                for (size_t i = 0; i < nparams && i < c->args.size(); ++i) {
                    auto aval = evaluate_expr(c->args[i].get(), tenv, venv, functions);
                    if (aval) local_venv[fn->params[i]] = *aval;
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
        auto v = evaluate_expr(as->value.get(), tenv, const_cast<ValueEnv&>(venv), functions);
        if (v) {
            // assign into venv
            const_cast<ValueEnv&>(venv)[as->target] = *v;
        }
        return std::nullopt;
    }
    if (auto es = dynamic_cast<const parser::ExprStmt*>(stmt)) {
        evaluate_expr(es->expr.get(), tenv, const_cast<ValueEnv&>(venv), functions);
        return std::nullopt;
    }
    if (auto rs = dynamic_cast<const parser::ReturnStmt*>(stmt)) {
        auto v = evaluate_expr(rs->value.get(), tenv, const_cast<ValueEnv&>(venv), functions);
        return v;
    }
    return std::nullopt;
}
