// cli_commands.cpp - CLI commands for single `cimple` tool
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <memory>
#include <unordered_map>
#include "frontend/lexer/lexer.h"
#include "frontend/lexer/token_utils.h"
#include "frontend/parser/parser.h"
#include "frontend/semantic/type_infer.h"
#include "frontend/eval/evaluator.h"

void handle_build(const std::string& path) {
	// simple pipeline: lex -> parse -> type inference -> report
	std::ifstream in(path);
	if (!in.is_open()) {
		std::cerr << "[cimple] Cannot open file: " << path << std::endl;
		return;
	}
	std::ostringstream buf;
	buf << in.rdbuf();
	std::string src = buf.str();

	auto tokens = cimple::lexer::lex(src);
	std::cout << "[cimple] Lexed " << tokens.size() << " tokens\n";

	cimple::parser::Parser p(tokens);
	auto module = p.parse_module();
	std::cout << "[cimple] Parsed module: " << module.body.size() << " top-level statements\n";

	auto env = cimple::semantic::infer_types(module);
	std::cout << "[cimple] Inferred types:\n";
	for (auto &kv: env.vars) {
		std::cout << "  var " << kv.first << " : " << cimple::semantic::type_to_string(kv.second) << "\n";
	}
	for (auto &kv: env.functions) {
		std::cout << "  func " << kv.first << " -> " << cimple::semantic::type_to_string(kv.second) << "\n";
	}
}

void handle_cli(int argc, char** argv) {
	// check for detailed flag anywhere in argv
	bool detailed = false;
	for (int i = 1; i < argc; ++i) if (std::string(argv[i]) == "--detailed-cli") detailed = true;

	if (argc >= 2 && std::string(argv[1]) == "-v"){
	std::cout<<"Cimple compiler 0.0.1 (dev)\n";
	return;
	}
	if (argc < 2 || (argc == 2 && detailed)) {
		// Start interactive REPL like python
		std::cout << "Cimple REPL (type 'exit' or 'quit' to leave)\n";
		cimple::semantic::TypeEnv cumulative_env;
		cimple::eval::ValueEnv cumulative_venv;

		// persistent function ASTs (owned here) and raw pointer map for evaluator
		std::unordered_map<std::string, std::unique_ptr<cimple::parser::FuncDef>> cumulative_funcs_owned;
		std::unordered_map<std::string, cimple::parser::FuncDef*> cumulative_funcs;
		std::string line;
		while (true) {
			std::cout << ">>> " << std::flush;
			if (!std::getline(std::cin, line)) break;
			if (line.empty()) continue;
			if (line == "exit" || line == "quit") break;

			// Lex + parse the single line
			auto tokens = cimple::lexer::lex(line + "\n");
				cimple::parser::Parser p(tokens);
				auto module = p.parse_module();

				// Print AST nodes
				for (auto &s: module.body) {
					if (s) std::cout << s->to_string() << "\n";
				}

				// Infer types for this input
				auto env = cimple::semantic::infer_types(module);

				// Evaluate statements and update cumulative value env when possible
				for (auto &s: module.body) {
					if (!s) continue;
					// handle function defs: persist AST across REPL iterations
					if (auto f = dynamic_cast<cimple::parser::FuncDef*>(s.get())) {
						// move ownership into cumulative_funcs_owned
						cumulative_funcs_owned[f->name] = std::unique_ptr<cimple::parser::FuncDef>(static_cast<cimple::parser::FuncDef*>(s.release()));
						cumulative_funcs[f->name] = cumulative_funcs_owned[f->name].get();
						// register type
						cumulative_env.functions[f->name] = env.functions[f->name];
						continue;
					}

					// assignment
					if (auto a = dynamic_cast<cimple::parser::AssignStmt*>(s.get())) {
						auto val = cimple::eval::evaluate_expr(a->value.get(), env, cumulative_venv, cumulative_funcs);
						if (val) {
							// store value
							cumulative_venv[a->target] = *val;
							// update cumulative types using value kind if type not provided
							cimple::semantic::TypeKind inferred = cimple::semantic::TypeKind::Unknown;
							if (val->kind == cimple::eval::Value::Int) inferred = cimple::semantic::TypeKind::Int;
							else if (val->kind == cimple::eval::Value::Float) inferred = cimple::semantic::TypeKind::Float;
							else if (val->kind == cimple::eval::Value::String) inferred = cimple::semantic::TypeKind::String;
							if (env.vars.count(a->target)) cumulative_env.vars[a->target] = env.vars[a->target];
							else cumulative_env.vars[a->target] = inferred;
						}
					}
					// expression statement: try to evaluate and print
					else if (auto ex = dynamic_cast<cimple::parser::ExprStmt*>(s.get())) {
						auto v = cimple::eval::evaluate_expr(ex->expr.get(), env, cumulative_venv, cumulative_funcs);
						if (v) std::cout << v->to_string() << std::endl;
					}
					else if (auto f = dynamic_cast<cimple::parser::FuncDef*>(s.get())) {
						// handled above when persisting
					}
				}

				// merge global vars/types from this input into cumulative_env
				for (auto &kv: env.vars) {
					auto it = cumulative_env.vars.find(kv.first);
					if (it == cumulative_env.vars.end()) cumulative_env.vars[kv.first] = kv.second;
					else {
						auto existing = it->second;
						auto incoming = kv.second;
						if (existing == cimple::semantic::TypeKind::Unknown) it->second = incoming;
						else if (incoming == cimple::semantic::TypeKind::Unknown) { /* keep existing */ }
						else if (existing != incoming) it->second = cimple::semantic::TypeKind::Unknown;
					}
				}
				for (auto &kv: env.functions) cumulative_env.functions[kv.first] = kv.second;

				// Print inferred cumulative types when detailed mode is enabled
				if (detailed) {
					for (auto &kv: cumulative_env.vars) {
						std::cout << "[type] " << kv.first << " : " << cimple::semantic::type_to_string(kv.second) << "\n";
					}
					for (auto &kv: cumulative_env.functions) {
						std::cout << "[func] " << kv.first << " -> " << cimple::semantic::type_to_string(kv.second) << "\n";
					}
				}
		}
		return;
	}

	std::string cmd = argv[1];
	if (cmd == "build") {
		if (argc < 3) {
			std::cout << "Usage: cimple build <file.cimp>\n";
			return;
		}
		handle_build(argv[2]);
	} else if (cmd == "lexparse" || cmd == "debug-lexparse") {
		if (argc < 3) {
			std::cout << "Usage: cimple lexparse <file.cimp>\n";
			return;
		}
		extern int lex_and_parse_file(const std::string&);
		lex_and_parse_file(argv[2]);
	} else {
		std::cout << "Unknown command: " << cmd << "\n";
	}
}
