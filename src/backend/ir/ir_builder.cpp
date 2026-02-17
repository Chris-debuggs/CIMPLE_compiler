// ir_builder.cpp - IR generation (placeholder - currently goes directly to LLVM)
#include "backend/ir/ir.h"

namespace cimple {
namespace backend {
namespace ir {

Module generate_ir(const parser::Module& ast_module, const semantic::TypeEnv& type_env) {
    // Placeholder - in a full implementation, this would build an internal IR
    // For now, we go directly AST -> LLVM IR
    Module irmod;
    return irmod;
}

} // namespace ir
} // namespace backend
} // namespace cimple
