#pragma once
#include "../parser/parser.h"
#include <string>
#include <unordered_map>

namespace cimple {
namespace semantic {

enum class TypeKind { Unknown, Int, Float, String, Bool, Void };

struct TypeEnv {
    std::unordered_map<std::string, TypeKind> vars;
    std::unordered_map<std::string, TypeKind> functions; // function return types
};

// Run simple type inference on a module. Returns TypeEnv with inferred types.
TypeEnv infer_types(const parser::Module& module);

std::string type_to_string(TypeKind t);

} // namespace semantic
} // namespace cimple
