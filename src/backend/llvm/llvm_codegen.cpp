// llvm_codegen.cpp - High-level LLVM code generation
#ifdef CIMPLE_USE_LLVM

#include "backend/llvm/llvm_codegen.h"
#include <llvm/Target/TargetMachine.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/CodeGen/Passes.h>
#include <llvm/IR/LegacyPassManager.h>

namespace cimple {
namespace backend {
namespace llvm {

CodeGenerator::CodeGenerator(const std::string& module_name)
    : context_(std::make_unique<LLVMContext>(module_name)),
      builder_(std::make_unique<ModuleBuilder>(*context_)),
      pass_manager_(nullptr) {
    // Initialize LLVM targets
    ::llvm::InitializeAllTargetInfos();
    ::llvm::InitializeAllTargets();
    ::llvm::InitializeAllTargetMCs();
    ::llvm::InitializeAllAsmParsers();
    ::llvm::InitializeAllAsmPrinters();
}

CodeGenerator::~CodeGenerator() = default;

void CodeGenerator::generate(const parser::Module& ast_module, const semantic::TypeEnv& type_env) {
    builder_->build_module(ast_module, type_env);
    pass_manager_ = std::make_unique<PassManager>(context_->get_module());
}

void CodeGenerator::optimize(int level) {
    if (pass_manager_) {
        pass_manager_->optimize_level(level);
    }
}

void CodeGenerator::emit_ir(const std::string& filename) {
    builder_->emit_ir_to_file(filename);
}

void CodeGenerator::emit_object(const std::string& filename) {
    // Emit object file using LLVM target backend
    std::string target_triple = ::llvm::sys::getDefaultTargetTriple();
    context_->get_module().setTargetTriple(target_triple);

    std::string error;
    const ::llvm::Target* target = ::llvm::TargetRegistry::lookupTarget(target_triple, error);
    if (!target) {
        // Fallback: just emit IR
        emit_ir(filename + ".ll");
        return;
    }

    ::llvm::TargetOptions opt;
    ::llvm::TargetMachine* target_machine = target->createTargetMachine(
        target_triple, "generic", "", opt, ::llvm::Reloc::PIC_
    );

    context_->get_module().setDataLayout(target_machine->createDataLayout());

    std::error_code ec;
    ::llvm::raw_fd_ostream dest(filename, ec, ::llvm::sys::fs::OF_None);
    if (ec) {
        emit_ir(filename + ".ll");
        delete target_machine;
        return;
    }

    ::llvm::legacy::PassManager pass;
    ::llvm::TargetMachine::CodeGenFileType file_type = ::llvm::TargetMachine::CGFT_ObjectFile;

    if (target_machine->addPassesToEmitFile(pass, dest, nullptr, file_type)) {
        emit_ir(filename + ".ll");
        delete target_machine;
        return;
    }

    pass.run(context_->get_module());
    dest.flush();

    delete target_machine;
}

} // namespace llvm
} // namespace backend
} // namespace cimple

#endif // CIMPLE_USE_LLVM
