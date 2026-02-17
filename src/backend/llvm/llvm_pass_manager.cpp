// llvm_pass_manager.cpp - LLVM optimization passes
#ifdef CIMPLE_USE_LLVM

#include "backend/llvm/llvm_pass_manager.h"
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Transforms/Utils.h>

namespace cimple {
namespace backend {
namespace llvm {

PassManager::PassManager(::llvm::Module& module)
    : module_(module),
      pass_builder_(),
      loop_am_(),
      function_am_(),
      cgscc_am_(),
      module_am_() {
    
    // Register analysis managers
    pass_builder_.registerModuleAnalyses(module_am_);
    pass_builder_.registerCGSCCAnalyses(cgscc_am_);
    pass_builder_.registerFunctionAnalyses(function_am_);
    pass_builder_.registerLoopAnalyses(loop_am_);
    pass_builder_.crossRegisterProxies(loop_am_, function_am_, cgscc_am_, module_am_);
}

void PassManager::optimize() {
    optimize_level(2); // Default optimization level
}

void PassManager::optimize_level(int level) {
    ::llvm::OptimizationLevel opt_level;
    
    switch (level) {
        case 0:
            opt_level = ::llvm::OptimizationLevel::O0;
            break;
        case 1:
            opt_level = ::llvm::OptimizationLevel::O1;
            break;
        case 2:
            opt_level = ::llvm::OptimizationLevel::O2;
            break;
        case 3:
            opt_level = ::llvm::OptimizationLevel::O3;
            break;
        default:
            opt_level = ::llvm::OptimizationLevel::O2;
    }

    ::llvm::ModulePassManager mpm = pass_builder_.buildPerModuleDefaultPipeline(opt_level);
    mpm.run(module_, module_am_);
}

} // namespace llvm
} // namespace backend
} // namespace cimple

#endif // CIMPLE_USE_LLVM
