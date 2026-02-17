#pragma once

#ifdef CIMPLE_USE_LLVM
#include <llvm/IR/Module.h>
#include <llvm/Passes/PassBuilder.h>
#include <memory>

namespace cimple {
namespace backend {
namespace llvm {

// Manages LLVM optimization passes
class PassManager {
public:
    PassManager(::llvm::Module& module);

    // Run optimization passes
    void optimize();

    // Run specific optimization level
    void optimize_level(int level); // 0 = no opt, 1 = less, 2 = default, 3 = aggressive

private:
    ::llvm::Module& module_;
    ::llvm::PassBuilder pass_builder_;
    ::llvm::LoopAnalysisManager loop_am_;
    ::llvm::FunctionAnalysisManager function_am_;
    ::llvm::CGSCCAnalysisManager cgscc_am_;
    ::llvm::ModuleAnalysisManager module_am_;
};

} // namespace llvm
} // namespace backend
} // namespace cimple

#endif // CIMPLE_USE_LLVM
