#include <iostream>
#include <fstream>
#include <sstream>
#include "../../include/frontend/lexer/lexer.h"
#include "../../include/frontend/lexer/token_utils.h"
#include "../../include/frontend/parser/parser.h"

int lex_and_parse_file(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        std::cerr << "Cannot open file: " << path << std::endl;
        return 1;
    }
    std::ostringstream buf;
    buf << in.rdbuf();
    std::string src = buf.str();

    auto tokens = cimple::lexer::lex(src);
    std::cout << "Tokens:\n";
    for (auto &t: tokens) {
        std::cout << cimple::lexer::token_to_string(t) << "\n";
    }

    cimple::parser::Parser p(tokens);
    auto module = p.parse_module();
    std::cout << "\nAST:\n";
    for (auto &s: module.body) {
        if (s) std::cout << s->to_string() << "\n";
    }
    return 0;
}
