#include "frontend/semantic/type_infer.h"
#include <iostream>

using namespace cimple;
using namespace cimple::semantic;

static TypeKind unify(TypeKind a, TypeKind b) {
    if (a == TypeKind::Unknown) return b;
    if (b == TypeKind::Unknown) return a;
    if (a == b) return a;
    if ((a == TypeKind::Int && b == TypeKind::Float) || (a == TypeKind::Float && b == TypeKind::Int)) return TypeKind::Float;
    return TypeKind::Unknown;
}

static TypeKind infer_expr(const parser::Expr* e, TypeEnv& env) {
    if (!e) return TypeKind::Unknown;
    if (auto n = dynamic_cast<const parser::NumberLiteral*>(e)) {
        return (n->value.find('.') != std::string::npos) ? TypeKind::Float : TypeKind::Int;
    }
    if (auto s = dynamic_cast<const parser::StringLiteral*>(e)) return TypeKind::String;
    if (auto v = dynamic_cast<const parser::VarRef*>(e)) {
        auto it = env.vars.find(v->name);
        if (it != env.vars.end()) return it->second;
        return TypeKind::Unknown;
    }
    if (auto b = dynamic_cast<const parser::BinaryOp*>(e)) {
        auto l = infer_expr(b->left.get(), env);
        auto r = infer_expr(b->right.get(), env);
        if (b->op == "+" && l == TypeKind::String && r == TypeKind::String) return TypeKind::String;
        return unify(l, r);
    }
    if (auto c = dynamic_cast<const parser::CallExpr*>(e)) {
        // try to resolve function return type
        if (auto vr = dynamic_cast<const parser::VarRef*>(c->callee.get())) {
            auto it = env.functions.find(vr->name);
            if (it != env.functions.end()) return it->second;
        }
        return TypeKind::Unknown;
    }
    return TypeKind::Unknown;
}

TypeEnv cimple::semantic::infer_types(const parser::Module& module) {
    TypeEnv env;
    // first pass: infer from top-level assignments and function bodies (function-scoped)
    for (auto &sptr : module.body) {
        if (!sptr) continue;
        if (auto a = dynamic_cast<parser::AssignStmt*>(sptr.get())) {
            TypeKind val = infer_expr(a->value.get(), env);
            env.vars[a->target] = val;
        }
        else if (auto f = dynamic_cast<parser::FuncDef*>(sptr.get())) {
            // create a local environment for the function
            TypeEnv local;
            // parameters are unknown initially
            for (auto &pname : f->params) local.vars[pname] = TypeKind::Unknown;

            // infer locals and return types in function scope
            TypeKind ret = TypeKind::Void;
            for (auto &bs : f->body) {
                if (auto as = dynamic_cast<parser::AssignStmt*>(bs.get())) {
                    TypeKind val = infer_expr(as->value.get(), local);
                    local.vars[as->target] = val;
                } else if (auto rs = dynamic_cast<parser::ReturnStmt*>(bs.get())) {
                    TypeKind t = infer_expr(rs->value.get(), local);
                    ret = unify(ret, t);
                }
            }

            env.functions[f->name] = ret;
            // do not leak local vars to global env
        }
    }

    return env;
}

std::string cimple::semantic::type_to_string(TypeKind t) {
    switch (t) {
        case TypeKind::Unknown: return "Unknown";
        case TypeKind::Int: return "int";
        case TypeKind::Float: return "float";
        case TypeKind::String: return "string";
        case TypeKind::Bool: return "bool";
        case TypeKind::Void: return "void";
    }
    return "?";
}
