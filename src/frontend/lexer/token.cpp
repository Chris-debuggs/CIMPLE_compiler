// lexer.cpp - basic lexer for the same syntax as Python
#include "frontend/lexer/lexer.h"
#include "frontend/lexer/token.h"
#include <cctype>
#include <sstream>

using namespace cimple::lexer;

static bool is_keyword(const std::string& s) {
    static const std::vector<std::string> kws = {
        "def","return","if","elif","else","for","while","in",
        "import","from","as","pass","break","continue","class",
        "and","or","not","True","False","None"
    };
    for (auto &k: kws) if (k == s) return true;
    return false;
}

std::vector<Token> cimple::lexer::lex(const std::string& source) {
    std::vector<Token> out;
    std::istringstream ss(source);
    std::string line;
    std::vector<int> indent_stack = {0};
    int lineno = 0;

    while (std::getline(ss, line)) {
        ++lineno;
        // convert tabs to 4 spaces for indentation counting
        std::string tline;
        for (char c: line) tline += (c=='\t' ? std::string(4,' ') : std::string(1,c));

        // count leading spaces
        size_t pos = 0;
        while (pos < tline.size() && (tline[pos] == ' ')) ++pos;
        // skip empty or comment-only lines
        size_t first_non_ws = tline.find_first_not_of(' ');
        if (first_non_ws == std::string::npos) continue;
        if (tline[first_non_ws] == '#') continue;

        int indent = static_cast<int>(pos);
        if (indent > indent_stack.back()) {
            indent_stack.push_back(indent);
            out.push_back({TokenType::INDENT, "", {lineno, 1}});
        } else {
            while (indent < indent_stack.back()) {
                indent_stack.pop_back();
                out.push_back({TokenType::DEDENT, "", {lineno, 1}});
            }
        }

        // tokenize the rest of the line
        size_t i = pos;
        int col = static_cast<int>(pos) + 1;
        while (i < tline.size()) {
            char c = tline[i];
            if (c == ' ' || c == '\r' || c == '\n') { ++i; ++col; continue; }
            if (c == '#') {
                // comment: consume rest
                std::string comment = tline.substr(i);
                out.push_back({TokenType::COMMENT, comment, {lineno, col}});
                break;
            }

            if (std::isalpha((unsigned char)c) || c == '_') {
                size_t j = i+1;
                while (j < tline.size() && (std::isalnum((unsigned char)tline[j]) || tline[j]=='_')) ++j;
                std::string ident = tline.substr(i, j-i);
                TokenType tt = is_keyword(ident) ? TokenType::KEYWORD : TokenType::IDENT;
                out.push_back({tt, ident, {lineno, col}});
                col += static_cast<int>(j-i);
                i = j;
                continue;
            }

            if (std::isdigit((unsigned char)c)) {
                size_t j = i+1;
                bool has_dot = false;
                while (j < tline.size() && (std::isdigit((unsigned char)tline[j]) || (!has_dot && tline[j]=='.'))) {
                    if (tline[j]=='.') has_dot = true;
                    ++j;
                }
                std::string num = tline.substr(i, j-i);
                out.push_back({TokenType::NUMBER, num, {lineno, col}});
                col += static_cast<int>(j-i);
                i = j;
                continue;
            }

            if (c == '"' || c == '\'') {
                char quote = c;
                size_t j = i+1;
                std::string buf;
                buf.push_back(quote);
                while (j < tline.size()) {
                    char cc = tline[j];
                    buf.push_back(cc);
                    if (cc == quote) { ++j; break; }
                    if (cc == '\\' && j+1 < tline.size()) { buf.push_back(tline[j+1]); j += 2; continue; }
                    ++j;
                }
                out.push_back({TokenType::STRING, buf, {lineno, col}});
                col += static_cast<int>(j-i);
                i = j;
                continue;
            }

            // operators and punctuation: treat single-char tokens as OP
            std::string op(1, c);
            // support two-char ops
            if (i+1 < tline.size()) {
                std::string two = tline.substr(i,2);
                static const std::vector<std::string> two_ops = {"==","!=","<=",">=","+=","-=","*=","/=","//","**","->","::","<<",">>"};
                for (auto &o: two_ops) if (o==two) { op = two; break; }
                if (op.size()==2) { out.push_back({TokenType::OP, op, {lineno, col}}); i += 2; col += 2; continue; }
            }
            out.push_back({TokenType::OP, op, {lineno, col}});
            ++i; ++col;
        }

        // end of logical line
        out.push_back({TokenType::NEWLINE, "", {lineno, static_cast<int>(tline.size())+1}});
    }

    // close remaining indents
    while (indent_stack.size() > 1) {
        indent_stack.pop_back();
        out.push_back({TokenType::DEDENT, "", {static_cast<int>(indent_stack.size()), 1}});
    }

    out.push_back({TokenType::ENDMARKER, "", {lineno+1, 1}});
    return out;
}

// Tokenize the input source string. Returns a vector of Tokens.
std::vector<Token> lex(const std::string& source);

// Convert a TokenType to a string.
std::string cimple::lexer::token_type_to_string(TokenType t) {
    switch (t) {
        case TokenType::INDENT: return "INDENT";
        case TokenType::DEDENT: return "DEDENT";
        case TokenType::NEWLINE: return "NEWLINE";
        case TokenType::ENDMARKER: return "ENDMARKER";
        case TokenType::IDENT: return "IDENT";
        case TokenType::NUMBER: return "NUMBER";
        case TokenType::STRING: return "STRING";
        case TokenType::OP: return "OP";
        case TokenType::KEYWORD: return "KEYWORD";
        case TokenType::COMMENT: return "COMMENT";
    }
    return "<unknown>";
}

// Convert a Token to a string.
std::string cimple::lexer::token_to_string(const Token& tok) {
    std::ostringstream ss;
    ss << token_type_to_string(tok.type) << " ";
    if (!tok.lexeme.empty()) ss << "('" << tok.lexeme << "') ";
    ss << "@" << tok.loc.line << ":" << tok.loc.column;
    return ss.str();
}

