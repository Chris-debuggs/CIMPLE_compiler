#pragma once

#ifdef CIMPLE_USE_LLVM
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Value.h>
#include "frontend/parser/parser.h"
#include "frontend/semantic/type_infer.h"
#include "llvm_context.h"
#include "llvm_type_mapper.h"
#include <memory>
#include <unordered_map>

namespace cimple {
namespace backend {
namespace llvm {

// Builds LLVM IR from AST
class ModuleBuilder {
public:
    ModuleBuilder(LLVMContext& llvm_ctx);

    // Generate LLVM IR from AST module
    void build_module(const parser::Module& ast_module, const semantic::TypeEnv& type_env);

    // Get the generated LLVM module
    ::llvm::Module& get_module() { return llvm_ctx_.get_module(); }

    // Emit LLVM IR to file
    void emit_ir_to_file(const std::string& filename);

private:
    LLVMContext& llvm_ctx_;
    TypeMapper type_mapper_;
    std::unique_ptr<::llvm::IRBuilder<>> builder_;
    
    // Symbol table for variables
    std::unordered_map<std::string, ::llvm::Value*> local_vars_;

    // Build a function from AST
    void build_function(const parser::FuncDef* func_def, const semantic::TypeEnv& type_env);

    // Build a statement
    void build_stmt(const parser::Stmt* stmt, const semantic::TypeEnv& type_env);

    // Build an expression and return LLVM Value
    ::llvm::Value* build_expr(const parser::Expr* expr, const semantic::TypeEnv& type_env);
};

} // namespace llvm
} // namespace backend
} // namespace cimple

#endif // CIMPLE_USE_LLVM
