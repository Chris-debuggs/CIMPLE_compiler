#pragma once

#include <string>

namespace cimple {
namespace lexer {

// Location in source (1-based)
struct SourceLocation {
    int line = 1;
    int column = 1;

    SourceLocation() = default;
    SourceLocation(int l, int c) : line(l), column(c) {}
};

// Core token kinds. Includes Python-style INDENT/DEDENT support.
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

// Token value
struct Token {
    TokenType type = TokenType::ENDMARKER;
    std::string lexeme;
    SourceLocation loc;

    Token() = default;
    Token(TokenType t, std::string l, SourceLocation s)
        : type(t), lexeme(std::move(l)), loc(s) {}
};

// Helpers
std::string token_type_to_string(TokenType t);
std::string token_to_string(const Token& tok);

} // namespace lexer
} // namespace cimple
