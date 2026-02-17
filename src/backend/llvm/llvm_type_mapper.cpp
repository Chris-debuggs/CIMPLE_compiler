// llvm_type_mapper.cpp - Type mapping implementation
#ifdef CIMPLE_USE_LLVM

#include "backend/llvm/llvm_type_mapper.h"
#include <llvm/IR/Type.h>

namespace cimple {
namespace backend {
namespace llvm {

::llvm::Type* TypeMapper::map_type(semantic::TypeKind kind) {
    switch (kind) {
        case semantic::TypeKind::Int:
            return ::llvm::Type::getInt32Ty(context_); // int32
        case semantic::TypeKind::Float:
            return ::llvm::Type::getDoubleTy(context_); // float64
        case semantic::TypeKind::String:
            // String is represented as a pointer to i8 (char*)
            return ::llvm::Type::getInt8PtrTy(context_);
        case semantic::TypeKind::Bool:
            return ::llvm::Type::getInt1Ty(context_);
        case semantic::TypeKind::Void:
            return ::llvm::Type::getVoidTy(context_);
        case semantic::TypeKind::Unknown:
        default:
            // Default to i32 for unknown types
            return ::llvm::Type::getInt32Ty(context_);
    }
}

} // namespace llvm
} // namespace backend
} // namespace cimple

#endif // CIMPLE_USE_LLVM
