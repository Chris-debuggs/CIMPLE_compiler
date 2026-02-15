#include "frontend/token_stream.h"
#include "frontend/lexer/lexer.h"
#include <stdexcept>

using namespace cimple;

TokenStream::TokenStream(const std::vector<lexer::Token>& toks)
    : tokens(toks), idx(0) {}

const lexer::Token& TokenStream::peek(size_t lookahead) const {
    size_t i = idx + lookahead;
    if (i >= tokens.size()) return tokens.back();
    return tokens[i];
}

lexer::Token TokenStream::next() {
    if (idx >= tokens.size()) return tokens.back();
    return tokens[idx++];
}

bool TokenStream::eof() const {
    if (tokens.empty()) return true;
    return tokens.back().type == lexer::TokenType::ENDMARKER && idx >= tokens.size()-1;
}

void TokenStream::rewind(size_t count) {
    if (count > idx) idx = 0; else idx -= count;
}
// token_stream.cpp - stub
