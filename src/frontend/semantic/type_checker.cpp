// type_checker.cpp - Static type checking implementation
#include "frontend/semantic/type_checker.h"
#include <sstream>

using namespace cimple;
using namespace cimple::semantic;

// Constructor is defined inline in type_checker.h

void TypeChecker::check() {
    errors_.clear();
    
    // Check all top-level statements
    for (const auto& stmt : module_.body) {
        if (stmt) {
            check_stmt(stmt.get(), type_env_);
        }
    }
    
    // If errors found, throw exception
    if (!errors_.empty()) {
        std::ostringstream msg;
        msg << "Type checking failed with " << errors_.size() << " error(s)";
        throw TypeCheckError(msg.str());
    }
}

std::vector<std::string> TypeChecker::get_errors() {
    errors_.clear();
    
    // Check all top-level statements
    for (const auto& stmt : module_.body) {
        if (stmt) {
            try {
                check_stmt(stmt.get(), type_env_);
            } catch (const TypeCheckError& e) {
                errors_.push_back(e.what());
            }
        }
    }
    
    return errors_;
}

void TypeChecker::check_stmt(const parser::Stmt* stmt, const TypeEnv& local_env) {
    if (!stmt) return;

    if (auto assign = dynamic_cast<const parser::AssignStmt*>(stmt)) {
        check_assignment(assign, local_env);
    }
    else if (auto expr_stmt = dynamic_cast<const parser::ExprStmt*>(stmt)) {
        // Expression statements are checked for their expression
        check_expr(expr_stmt->expr.get(), local_env);
    }
    else if (auto ret = dynamic_cast<const parser::ReturnStmt*>(stmt)) {
        // Return statement - check expression type
        check_expr(ret->value.get(), local_env);
    }
    else if (auto func_def = dynamic_cast<const parser::FuncDef*>(stmt)) {
        // Function definition - create local environment with parameters
        TypeEnv func_env = local_env;
        for (const auto& param : func_def->params) {
            // Parameters start as Unknown, will be inferred from usage
            func_env.vars[param] = TypeKind::Unknown;
        }
        
        // Check function body
        for (const auto& body_stmt : func_def->body) {
            if (body_stmt) {
                check_stmt(body_stmt.get(), func_env);
            }
        }
    }
}

TypeKind TypeChecker::check_expr(const parser::Expr* expr, const TypeEnv& local_env) {
    if (!expr) return TypeKind::Unknown;

    if (auto num = dynamic_cast<const parser::NumberLiteral*>(expr)) {
        return (num->value.find('.') != std::string::npos) ? TypeKind::Float : TypeKind::Int;
    }
    
    if (auto str = dynamic_cast<const parser::StringLiteral*>(expr)) {
        return TypeKind::String;
    }
    
    if (auto var_ref = dynamic_cast<const parser::VarRef*>(expr)) {
        // Look up variable type
        auto it = local_env.vars.find(var_ref->name);
        if (it != local_env.vars.end()) {
            return it->second;
        }
        // Check global env
        auto global_it = type_env_.vars.find(var_ref->name);
        if (global_it != type_env_.vars.end()) {
            return global_it->second;
        }
        // Variable not found - this is a semantic error, but we'll return Unknown for now
        return TypeKind::Unknown;
    }
    
    if (auto bin_op = dynamic_cast<const parser::BinaryOp*>(expr)) {
        TypeKind left_type = check_expr(bin_op->left.get(), local_env);
        TypeKind right_type = check_expr(bin_op->right.get(), local_env);
        
        // Check binary operation compatibility
        check_binary_op(bin_op, left_type, right_type, get_location(bin_op));
        
        // Return result type
        if (bin_op->op == "+" && left_type == TypeKind::String && right_type == TypeKind::String) {
            return TypeKind::String;
        }
        // Numeric operations
        if ((left_type == TypeKind::Int || left_type == TypeKind::Float) &&
            (right_type == TypeKind::Int || right_type == TypeKind::Float)) {
            // Promote to float if either operand is float
            return (left_type == TypeKind::Float || right_type == TypeKind::Float) 
                ? TypeKind::Float : TypeKind::Int;
        }
        return TypeKind::Unknown;
    }
    
    if (auto call = dynamic_cast<const parser::CallExpr*>(expr)) {
        check_call(call, local_env);
        // Try to get return type from function
        if (auto callee_var = dynamic_cast<const parser::VarRef*>(call->callee.get())) {
            auto it = local_env.functions.find(callee_var->name);
            if (it != local_env.functions.end()) {
                return it->second;
            }
            auto global_it = type_env_.functions.find(callee_var->name);
            if (global_it != type_env_.functions.end()) {
                return global_it->second;
            }
        }
        return TypeKind::Unknown;
    }
    
    return TypeKind::Unknown;
}

void TypeChecker::check_binary_op(const parser::BinaryOp* op, TypeKind left_type, TypeKind right_type, lexer::SourceLocation loc) {
    // String concatenation: only allow string + string
    if (op->op == "+") {
        if (left_type == TypeKind::String && right_type != TypeKind::String) {
            std::ostringstream msg;
            msg << "Cannot concatenate string with non-string type";
            errors_.push_back(msg.str());
            throw TypeCheckError(msg.str(), loc);
        }
        if (right_type == TypeKind::String && left_type != TypeKind::String) {
            std::ostringstream msg;
            msg << "Cannot concatenate non-string type with string";
            errors_.push_back(msg.str());
            throw TypeCheckError(msg.str(), loc);
        }
    }
    
    // Numeric operations: only allow numeric types
    if (op->op == "+" || op->op == "-" || op->op == "*" || op->op == "/") {
        bool left_numeric = (left_type == TypeKind::Int || left_type == TypeKind::Float);
        bool right_numeric = (right_type == TypeKind::Int || right_type == TypeKind::Float);
        
        if (!left_numeric && left_type != TypeKind::Unknown) {
            std::ostringstream msg;
            msg << "Left operand of '" << op->op << "' must be numeric, got " << type_to_string(left_type);
            errors_.push_back(msg.str());
            throw TypeCheckError(msg.str(), loc);
        }
        
        if (!right_numeric && right_type != TypeKind::Unknown) {
            std::ostringstream msg;
            msg << "Right operand of '" << op->op << "' must be numeric, got " << type_to_string(right_type);
            errors_.push_back(msg.str());
            throw TypeCheckError(msg.str(), loc);
        }
        
        // If both are strings and op is not +, error
        if (left_type == TypeKind::String && right_type == TypeKind::String && op->op != "+") {
            std::ostringstream msg;
            msg << "Operation '" << op->op << "' not supported for string types";
            errors_.push_back(msg.str());
            throw TypeCheckError(msg.str(), loc);
        }
    }
}

void TypeChecker::check_call(const parser::CallExpr* call, const TypeEnv& local_env) {
    if (!call || !call->callee) return;
    
    // For now, we don't have function signatures with parameter types
    // So we can't fully validate argument types
    // This is a placeholder for future enhancement
    
    // Check that all arguments can be type-checked
    for (const auto& arg : call->args) {
        check_expr(arg.get(), local_env);
    }
}

void TypeChecker::check_assignment(const parser::AssignStmt* assign, const TypeEnv& local_env) {
    if (!assign) return;
    
    TypeKind value_type = check_expr(assign->value.get(), local_env);
    
    // Check if variable already exists with a different type
    auto it = local_env.vars.find(assign->target);
    if (it != local_env.vars.end()) {
        TypeKind existing_type = it->second;
        if (existing_type != TypeKind::Unknown && value_type != TypeKind::Unknown && 
            existing_type != value_type) {
            std::ostringstream msg;
            msg << "Cannot assign " << type_to_string(value_type) 
                << " to variable '" << assign->target << "' of type " << type_to_string(existing_type);
            errors_.push_back(msg.str());
            throw TypeCheckError(msg.str(), get_location(assign));
        }
    }
    
    // Check global env
    auto global_it = type_env_.vars.find(assign->target);
    if (global_it != type_env_.vars.end()) {
        TypeKind existing_type = global_it->second;
        if (existing_type != TypeKind::Unknown && value_type != TypeKind::Unknown && 
            existing_type != value_type) {
            std::ostringstream msg;
            msg << "Cannot assign " << type_to_string(value_type) 
                << " to variable '" << assign->target << "' of type " << type_to_string(existing_type);
            errors_.push_back(msg.str());
            throw TypeCheckError(msg.str(), get_location(assign));
        }
    }
}

lexer::SourceLocation TypeChecker::get_location(const parser::Node* node) {
    // For now, return default location
    // In a full implementation, AST nodes would store source locations
    return {0, 0};
}

namespace cimple {
namespace semantic {

bool check_types(const parser::Module& module, const TypeEnv& type_env, std::vector<std::string>& errors) {
    TypeChecker checker(module, type_env);
    errors = checker.get_errors();
    return errors.empty();
}

} // namespace semantic
} // namespace cimple
