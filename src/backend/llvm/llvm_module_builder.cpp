// llvm_module_builder.cpp - LLVM IR generation from AST
#ifdef CIMPLE_USE_LLVM

#include "backend/llvm/llvm_module_builder.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>
#include <fstream>

namespace cimple {
namespace backend {
namespace llvm {

ModuleBuilder::ModuleBuilder(LLVMContext& llvm_ctx)
    : llvm_ctx_(llvm_ctx),
      type_mapper_(llvm_ctx.get_context()),
      builder_(std::make_unique<::llvm::IRBuilder<>>(llvm_ctx.get_context())) {
}

void ModuleBuilder::build_module(const parser::Module& ast_module, const semantic::TypeEnv& type_env) {
    local_vars_.clear();

    // Build all top-level functions
    for (const auto& stmt : ast_module.body) {
        if (auto func_def = dynamic_cast<const parser::FuncDef*>(stmt.get())) {
            build_function(func_def, type_env);
        }
    }
}

void ModuleBuilder::build_function(const parser::FuncDef* func_def, const semantic::TypeEnv& type_env) {
    if (!func_def) return;

    // Get return type
    auto ret_type_it = type_env.functions.find(func_def->name);
    semantic::TypeKind ret_type_kind = (ret_type_it != type_env.functions.end()) 
        ? ret_type_it->second : semantic::TypeKind::Void;
    
    ::llvm::Type* ret_type = type_mapper_.map_type(ret_type_kind);

    // Build parameter types
    std::vector<::llvm::Type*> param_types;
    for (const auto& param_name : func_def->params) {
        // For now, assume all parameters are int32
        // In a full implementation, we'd look up parameter types from function signature
        param_types.push_back(::llvm::Type::getInt32Ty(type_mapper_.get_context()));
    }

    // Create function type
    ::llvm::FunctionType* func_type = ::llvm::FunctionType::get(ret_type, param_types, false);

    // Create function
    ::llvm::Function* func = ::llvm::Function::Create(
        func_type,
        ::llvm::Function::ExternalLinkage,
        func_def->name,
        &llvm_ctx_.get_module()
    );

    // Create entry basic block
    ::llvm::BasicBlock* entry_block = ::llvm::BasicBlock::Create(type_mapper_.get_context(), "entry", func);
    builder_->SetInsertPoint(entry_block);

    // Set up local variable map for function parameters
    local_vars_.clear();
    size_t param_idx = 0;
    for (auto& arg : func->args()) {
        if (param_idx < func_def->params.size()) {
            local_vars_[func_def->params[param_idx]] = &arg;
            param_idx++;
        }
    }

    // Build function body
    for (const auto& body_stmt : func_def->body) {
        if (body_stmt) {
            build_stmt(body_stmt.get(), type_env);
        }
    }

    // If function doesn't return, add implicit return
    if (ret_type_kind == semantic::TypeKind::Void) {
        builder_->CreateRetVoid();
    } else {
        // Return default value for non-void functions without explicit return
        ::llvm::Value* default_val = nullptr;
        if (ret_type_kind == semantic::TypeKind::Int) {
            default_val = ::llvm::ConstantInt::get(ret_type, 0);
        } else if (ret_type_kind == semantic::TypeKind::Float) {
            default_val = ::llvm::ConstantFP::get(ret_type, 0.0);
        } else {
            default_val = ::llvm::ConstantInt::get(ret_type, 0);
        }
        if (default_val) {
            builder_->CreateRet(default_val);
        }
    }

    // Verify function
    ::llvm::verifyFunction(*func);
}

void ModuleBuilder::build_stmt(const parser::Stmt* stmt, const semantic::TypeEnv& type_env) {
    if (!stmt) return;

    if (auto assign = dynamic_cast<const parser::AssignStmt*>(stmt)) {
        ::llvm::Value* value = build_expr(assign->value.get(), type_env);
        if (value) {
            local_vars_[assign->target] = value;
        }
    }
    else if (auto ret = dynamic_cast<const parser::ReturnStmt*>(stmt)) {
        ::llvm::Value* ret_val = build_expr(ret->value.get(), type_env);
        if (ret_val) {
            builder_->CreateRet(ret_val);
        }
    }
    else if (auto expr_stmt = dynamic_cast<const parser::ExprStmt*>(stmt)) {
        // Expression statements are evaluated but result is discarded
        build_expr(expr_stmt->expr.get(), type_env);
    }
}

::llvm::Value* ModuleBuilder::build_expr(const parser::Expr* expr, const semantic::TypeEnv& type_env) {
    if (!expr) return nullptr;

    if (auto num = dynamic_cast<const parser::NumberLiteral*>(expr)) {
        if (num->value.find('.') != std::string::npos) {
            // Float literal
            double val = std::stod(num->value);
            return ::llvm::ConstantFP::get(type_mapper_.get_context(), ::llvm::APFloat(val));
        } else {
            // Integer literal
            int64_t val = std::stoll(num->value);
            return ::llvm::ConstantInt::get(type_mapper_.get_context(), ::llvm::APInt(32, val, true));
        }
    }

    if (auto str = dynamic_cast<const parser::StringLiteral*>(expr)) {
        // Create global string constant
        ::llvm::Constant* str_const = ::llvm::ConstantDataArray::getString(
            type_mapper_.get_context(),
            str->value
        );
        ::llvm::GlobalVariable* global_str = new ::llvm::GlobalVariable(
            llvm_ctx_.get_module(),
            str_const->getType(),
            true, // isConstant
            ::llvm::GlobalValue::PrivateLinkage,
            str_const,
            ".str"
        );
        // Get pointer to first element
        return builder_->CreateInBoundsGEP(
            global_str->getValueType(),
            global_str,
            {
                ::llvm::ConstantInt::get(type_mapper_.get_context(), ::llvm::APInt(32, 0)),
                ::llvm::ConstantInt::get(type_mapper_.get_context(), ::llvm::APInt(32, 0))
            }
        );
    }

    if (auto var_ref = dynamic_cast<const parser::VarRef*>(expr)) {
        auto it = local_vars_.find(var_ref->name);
        if (it != local_vars_.end()) {
            return it->second;
        }
        return nullptr;
    }

    if (auto bin_op = dynamic_cast<const parser::BinaryOp*>(expr)) {
        ::llvm::Value* left = build_expr(bin_op->left.get(), type_env);
        ::llvm::Value* right = build_expr(bin_op->right.get(), type_env);
        
        if (!left || !right) return nullptr;

        if (bin_op->op == "+") {
            // Check if both are integers or floats
            if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
                return builder_->CreateAdd(left, right, "addtmp");
            } else if (left->getType()->isFloatingPointTy() || right->getType()->isFloatingPointTy()) {
                // Promote to float if needed
                if (left->getType()->isIntegerTy()) {
                    left = builder_->CreateSIToFP(left, ::llvm::Type::getDoubleTy(type_mapper_.get_context()));
                }
                if (right->getType()->isIntegerTy()) {
                    right = builder_->CreateSIToFP(right, ::llvm::Type::getDoubleTy(type_mapper_.get_context()));
                }
                return builder_->CreateFAdd(left, right, "addtmp");
            }
        } else if (bin_op->op == "-") {
            if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
                return builder_->CreateSub(left, right, "subtmp");
            } else {
                if (left->getType()->isIntegerTy()) {
                    left = builder_->CreateSIToFP(left, ::llvm::Type::getDoubleTy(type_mapper_.get_context()));
                }
                if (right->getType()->isIntegerTy()) {
                    right = builder_->CreateSIToFP(right, ::llvm::Type::getDoubleTy(type_mapper_.get_context()));
                }
                return builder_->CreateFSub(left, right, "subtmp");
            }
        } else if (bin_op->op == "*") {
            if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
                return builder_->CreateMul(left, right, "multmp");
            } else {
                if (left->getType()->isIntegerTy()) {
                    left = builder_->CreateSIToFP(left, ::llvm::Type::getDoubleTy(type_mapper_.get_context()));
                }
                if (right->getType()->isIntegerTy()) {
                    right = builder_->CreateSIToFP(right, ::llvm::Type::getDoubleTy(type_mapper_.get_context()));
                }
                return builder_->CreateFMul(left, right, "multmp");
            }
        } else if (bin_op->op == "/") {
            if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
                return builder_->CreateSDiv(left, right, "divtmp");
            } else {
                if (left->getType()->isIntegerTy()) {
                    left = builder_->CreateSIToFP(left, ::llvm::Type::getDoubleTy(type_mapper_.get_context()));
                }
                if (right->getType()->isIntegerTy()) {
                    right = builder_->CreateSIToFP(right, ::llvm::Type::getDoubleTy(type_mapper_.get_context()));
                }
                return builder_->CreateFDiv(left, right, "divtmp");
            }
        }
    }

    if (auto call = dynamic_cast<const parser::CallExpr*>(expr)) {
        if (auto callee_var = dynamic_cast<const parser::VarRef*>(call->callee.get())) {
            ::llvm::Function* func = llvm_ctx_.get_module().getFunction(callee_var->name);
            if (func) {
                std::vector<::llvm::Value*> args;
                for (const auto& arg_expr : call->args) {
                    ::llvm::Value* arg_val = build_expr(arg_expr.get(), type_env);
                    if (arg_val) args.push_back(arg_val);
                }
                return builder_->CreateCall(func, args, "calltmp");
            }
        }
    }

    return nullptr;
}

void ModuleBuilder::emit_ir_to_file(const std::string& filename) {
    std::error_code ec;
    ::llvm::raw_fd_ostream out(filename, ec);
    if (!ec) {
        llvm_ctx_.get_module().print(out, nullptr);
    }
}

} // namespace llvm
} // namespace backend
} // namespace cimple

#endif // CIMPLE_USE_LLVM
