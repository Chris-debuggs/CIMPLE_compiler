#pragma once

#ifdef CIMPLE_USE_LLVM
#include "llvm_context.h"
#include "llvm_module_builder.h"
#include "llvm_pass_manager.h"
#include "../../frontend/parser/parser.h"
#include "../../frontend/semantic/type_infer.h"
#include <string>

namespace cimple {
namespace backend {
namespace llvm {

// High-level LLVM code generation interface
class CodeGenerator {
public:
    CodeGenerator(const std::string& module_name);
    ~CodeGenerator();

    // Generate LLVM IR from AST
    void generate(const parser::Module& ast_module, const semantic::TypeEnv& type_env);

    // Optimize the generated IR
    void optimize(int level = 2);

    // Emit LLVM IR to file
    void emit_ir(const std::string& filename);

    // Emit object file (requires LLVM target backend)
    void emit_object(const std::string& filename);

private:
    std::unique_ptr<LLVMContext> context_;
    std::unique_ptr<ModuleBuilder> builder_;
    std::unique_ptr<PassManager> pass_manager_;
};

} // namespace llvm
} // namespace backend
} // namespace cimple

#endif // CIMPLE_USE_LLVM
