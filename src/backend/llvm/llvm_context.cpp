// llvm_context.cpp - LLVM context implementation
#ifdef CIMPLE_USE_LLVM

#include "backend/llvm/llvm_context.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

namespace cimple {
namespace backend {
namespace llvm {

LLVMContext::LLVMContext(const std::string& module_name)
    : context_(std::make_unique<::llvm::LLVMContext>()),
      module_(std::make_unique<::llvm::Module>(module_name, *context_)) {
}

LLVMContext::~LLVMContext() = default;

} // namespace llvm
} // namespace backend
} // namespace cimple

#endif // CIMPLE_USE_LLVM
