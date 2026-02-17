#pragma once
#include "frontend/lexer/token.h"
#include <string>
#include <string_view>
#include <vector>

namespace cimple {
namespace lexer {

// Tokenize the input source string. Returns a vector of Tokens.
// Uses std::string_view internally for zero-copy performance.
std::vector<Token> lex(const std::string& source);

// Internal helper: tokenize from string_view (avoids copies)
std::vector<Token> lex_from_view(std::string_view source);

} // namespace lexer
} // namespace cimple
