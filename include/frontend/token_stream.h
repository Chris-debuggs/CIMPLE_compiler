#pragma once
#include "frontend/lexer/lexer.h"
#include <vector>
#include <cstddef>

namespace cimple {

class TokenStream {
public:
    TokenStream(const std::vector<lexer::Token>& toks);

    const lexer::Token& peek(size_t lookahead = 0) const;
    lexer::Token next();
    bool eof() const;
    void rewind(size_t count = 1);

private:
    std::vector<lexer::Token> tokens;
    size_t idx = 0;
};

} // namespace cimple
