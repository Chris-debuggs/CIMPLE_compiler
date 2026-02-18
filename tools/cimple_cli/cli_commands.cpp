// cli_commands.cpp - CLI commands for single `cimple` tool
#include "frontend/eval/evaluator.h"
#include "frontend/lexer/lexer.h"
#include "frontend/lexer/token_utils.h"
#include "frontend/parser/parser.h"
#include "frontend/semantic/type_checker.h"
#include "frontend/semantic/type_infer.h"
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef CIMPLE_USE_LLVM
#include "backend/llvm/llvm_codegen.h"
#endif
#include "backend/ir/ir.h"
#include "driver/linker_driver.h"

void handle_build(const std::string &path) {
  // simple pipeline: lex -> parse -> type inference -> report
  std::ifstream in(path);
  if (!in.is_open()) {
    std::cerr << "[cimple] Cannot open file: " << path << std::endl;
    return;
  }
  std::ostringstream buf;
  buf << in.rdbuf();
  std::string source = buf.str();
  auto tokens = cimple::lexer::lex(source);
  std::cout << "[cimple] Lexed " << tokens.size() << " tokens\n";

  cimple::parser::Parser p(tokens);
  auto module = p.parse_module();
  std::cout << "[cimple] Parsed module: " << module.body.size()
            << " top-level statements\n";

  auto env = cimple::semantic::infer_types(module);
  std::cout << "[cimple] Inferred types:\n";
  for (auto &kv : env.vars) {
    std::cout << "  var " << kv.first << " : "
              << cimple::semantic::type_to_string(kv.second) << "\n";
  }
  for (auto &kv : env.functions) {
    std::cout << "  func " << kv.first << " -> "
              << cimple::semantic::type_to_string(kv.second) << "\n";
  }

  // Run static type checking
  std::cout << "[cimple] Running type checker...\n";
  std::vector<std::string> type_errors;
  if (!cimple::semantic::check_types(module, env, type_errors)) {
    std::cerr << "[cimple] Type checking failed:\n";
    for (const auto &error : type_errors) {
      std::cerr << "  ERROR: " << error << "\n";
    }
    return; // Stop compilation on type errors
  }
  std::cout << "[cimple] Type checking passed\n";

  // Generate LLVM IR and emit object file
#ifdef CIMPLE_USE_LLVM
  {
    std::string base = path;
    size_t dot = base.find_last_of('.');
    if (dot != std::string::npos)
      base = base.substr(0, dot);

    std::cout << "[cimple] Generating LLVM IR...\n";
    cimple::backend::llvm::CodeGenerator codegen(base);
    codegen.generate(module, env);

    std::cout << "[cimple] Optimizing LLVM IR...\n";
    codegen.optimize(2); // O2 optimization

    std::string ir_file = base + ".ll";
    std::cout << "[cimple] Emitting LLVM IR to " << ir_file << "\n";
    codegen.emit_ir(ir_file);

    std::string obj_file = base;
#ifdef _WIN32
    obj_file += ".obj";
#else
    obj_file += ".o";
#endif
    std::cout << "[cimple] Emitting object file to " << obj_file << "\n";
    codegen.emit_object(obj_file);

    std::cout << "[cimple] Compilation complete.\n";

    // Link object file into executable
    std::cout << "[cimple] Linking executable...\n";
    cimple::driver::LinkerDriver linker;
    linker.add_object_file(obj_file);
    std::string exe_name = base;
#ifdef _WIN32
    exe_name += ".exe";
#endif
    linker.set_output(exe_name);
    linker.enable_dead_code_elimination(true);
    if (linker.link()) {
      std::cout << "[cimple] Successfully created executable: " << exe_name
                << "\n";
    } else {
      std::cerr << "[cimple] Linking failed. Object file available at: "
                << obj_file << "\n";
    }
  }
#else
  std::cout << "[cimple] LLVM backend not enabled. Install LLVM and rebuild "
               "with CIMPLE_USE_LLVM=ON\n";
#endif
}

// Old emit_and_link_with_clang function removed - replaced with LLVM backend
// codegen

void handle_run(const std::string &path) {
  std::ifstream in(path);
  if (!in.is_open()) {
    std::cerr << "[cimple] Cannot open file: " << path << std::endl;
    return;
  }
  std::ostringstream buf;
  buf << in.rdbuf();
  std::string source = buf.str();

  auto tokens = cimple::lexer::lex(source);

  cimple::parser::Parser p(tokens);
  auto module = p.parse_module();

  auto env = cimple::semantic::infer_types(module);

  // Build function table for evaluator
  std::unordered_map<std::string, cimple::parser::FuncDef *> functions;
  for (auto &stmt : module.body) {
    if (auto fn = dynamic_cast<cimple::parser::FuncDef *>(stmt.get())) {
      functions[fn->name] = fn;
    }
  }

  // Execute top-level statements
  cimple::eval::ValueEnv venv;
  for (auto &stmt : module.body) {
    cimple::eval::evaluate_stmt(stmt.get(), env, venv, functions);
  }
}

void handle_cli(int argc, char **argv) {
  if (argc < 2) {
    std::cout << "Usage: cimple <command> <file.cimp>\n";
    std::cout << "Commands:\n";
    std::cout
        << "  build <file>     Compile to native binary (requires LLVM)\n";
    std::cout << "  run <file>       Run via interpreter (no LLVM needed)\n";
    std::cout << "  lexparse <file>  Debug: lex and parse only\n";
    return;
  }

  std::string cmd = argv[1];
  if (cmd == "build") {
    if (argc < 3) {
      std::cout << "Usage: cimple build <file.cimp>\n";
      return;
    }
    handle_build(argv[2]);
  } else if (cmd == "run") {
    if (argc < 3) {
      std::cout << "Usage: cimple run <file.cimp>\n";
      return;
    }
    handle_run(argv[2]);
  } else if (cmd == "lexparse" || cmd == "debug-lexparse") {
    if (argc < 3) {
      std::cout << "Usage: cimple lexparse <file.cimp>\n";
      return;
    }
    extern int lex_and_parse_file(const std::string &);
    lex_and_parse_file(argv[2]);
  } else {
    std::cout << "Unknown command: " << cmd << "\n";
  }
}
