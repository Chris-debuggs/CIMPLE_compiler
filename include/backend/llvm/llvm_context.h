#pragma once

#ifdef CIMPLE_USE_LLVM
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <memory>

namespace cimple {
namespace backend {
namespace llvm {

// LLVM context wrapper - manages LLVM context and module
class LLVMContext {
public:
    LLVMContext(const std::string& module_name);
    ~LLVMContext();

    ::llvm::LLVMContext& get_context() { return *context_; }
    ::llvm::Module& get_module() { return *module_; }

private:
    std::unique_ptr<::llvm::LLVMContext> context_;
    std::unique_ptr<::llvm::Module> module_;
};

} // namespace llvm
} // namespace backend
} // namespace cimple

#endif // CIMPLE_USE_LLVM
