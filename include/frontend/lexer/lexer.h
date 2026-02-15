#pragma once
#include <string>
#include <vector>

namespace cimple {
namespace lexer {

enum class TokenType {
    INDENT,
    DEDENT,
    NEWLINE,
    ENDMARKER,
    IDENT,
    NUMBER,
    STRING,
    OP,
    KEYWORD,
    COMMENT,
};

struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int column;
};

// Tokenize the input source string. Returns a vector of Tokens.
std::vector<Token> lex(const std::string& source);

} // namespace lexer
} // namespace cimple
