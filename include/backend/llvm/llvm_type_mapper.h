#pragma once

#ifdef CIMPLE_USE_LLVM
#include <llvm/IR/Type.h>
#include <llvm/IR/LLVMContext.h>
#include "frontend/semantic/type_infer.h"

namespace cimple {
namespace backend {
namespace llvm {

// Maps Cimple types to LLVM types
class TypeMapper {
public:
    TypeMapper(::llvm::LLVMContext& context) : context_(context) {}

    // Map Cimple TypeKind to LLVM Type
    ::llvm::Type* map_type(semantic::TypeKind kind);

    // Get LLVM context
    ::llvm::LLVMContext& get_context() { return context_; }

private:
    ::llvm::LLVMContext& context_;
};

} // namespace llvm
} // namespace backend
} // namespace cimple

#endif // CIMPLE_USE_LLVM
