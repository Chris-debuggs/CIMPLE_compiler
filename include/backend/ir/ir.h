#pragma once

#include "../../frontend/parser/parser.h"
#include "../../frontend/semantic/type_infer.h"

namespace cimple {
namespace backend {
namespace ir {

// IR Module - placeholder for internal IR representation
// Currently, we go directly AST -> LLVM IR
struct Module {
    // Placeholder structure
    // In a full implementation, this would contain internal IR nodes
};

// Generate IR from AST (placeholder - currently goes directly to LLVM)
Module generate_ir(const parser::Module& ast_module, const semantic::TypeEnv& type_env);

} // namespace ir
} // namespace backend
} // namespace cimple
