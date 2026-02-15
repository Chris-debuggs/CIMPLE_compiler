#pragma once
#include "lexer.h"
#include <string>

namespace cimple {
namespace lexer {

std::string token_type_to_string(TokenType t);
std::string token_to_string(const Token& tok);

} // namespace lexer
} // namespace cimple
