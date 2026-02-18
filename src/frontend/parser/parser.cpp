// parser.cpp - Recursive descent parser
// TODO: Align with Python's official PEG grammar (python.gram)
// See docs/PEG_GRAMMAR_PLAN.md for implementation plan
#include "frontend/parser/parser.h"
#include "frontend/lexer/token_utils.h"
#include <iostream>

using namespace cimple;
using namespace cimple::parser;

Parser::Parser(const std::vector<lexer::Token>& tokens) : ts(tokens) {}

Module Parser::parse_module() {
    Module m;
    while (!ts.eof()) {
        auto s = parse_statement();
        if (s) m.body.push_back(std::move(s));
        else break;
    }
    return m;
}

std::unique_ptr<Stmt> Parser::parse_statement() {
    auto t = ts.peek();
    if (t.type == lexer::TokenType::KEYWORD && t.lexeme == "def") {
        return parse_funcdef();
    }
    if (t.type == lexer::TokenType::KEYWORD && t.lexeme == "return") {
        ts.next();
        auto val = parse_expression();
        if (ts.peek().type == lexer::TokenType::NEWLINE) ts.next();
        return std::make_unique<ReturnStmt>(std::move(val));
    }
    return parse_simple_statement();
}

std::unique_ptr<FuncDef> Parser::parse_funcdef() {
    ts.next(); // def
    auto nameTok = ts.next();
    if (nameTok.type != lexer::TokenType::IDENT) {
        std::cerr << "Parser error: expected function name" << std::endl;
        return nullptr;
    }
    std::string name = nameTok.lexeme;

    if (ts.peek().type == lexer::TokenType::OP && ts.peek().lexeme == "(") ts.next();
    std::vector<std::string> params;
    while (!ts.eof() && !(ts.peek().type == lexer::TokenType::OP && ts.peek().lexeme == ")")) {
        auto tok = ts.next();
        if (tok.type == lexer::TokenType::IDENT) params.push_back(tok.lexeme);
        if (ts.peek().type == lexer::TokenType::OP && ts.peek().lexeme == ",") ts.next();
    }
    if (ts.peek().type == lexer::TokenType::OP && ts.peek().lexeme == ")") ts.next();

    while (!ts.eof() && ts.peek().type != lexer::TokenType::NEWLINE) ts.next();
    if (ts.peek().type == lexer::TokenType::NEWLINE) ts.next();
    if (ts.peek().type == lexer::TokenType::INDENT) ts.next();

    auto fn = std::make_unique<FuncDef>();
    fn->name = name;
    fn->params = std::move(params);

    while (!ts.eof() && ts.peek().type != lexer::TokenType::DEDENT) {
        auto stmt = parse_statement();
        if (stmt) fn->body.push_back(std::move(stmt));
        else break;
    }
    if (ts.peek().type == lexer::TokenType::DEDENT) ts.next();
    return fn;
}

std::unique_ptr<Stmt> Parser::parse_simple_statement() {
    auto t = ts.peek();
    if (t.type == lexer::TokenType::NEWLINE) { ts.next(); return nullptr; }
    if (t.type == lexer::TokenType::ENDMARKER) return nullptr;
    // Skip bare INDENT/DEDENT tokens at statement level (can appear in top-level code)
    if (t.type == lexer::TokenType::INDENT || t.type == lexer::TokenType::DEDENT) { ts.next(); return nullptr; }

    auto expr = parse_expression();
    if (!expr) return nullptr;  // parse_factor returned nullptr for unrecognized token
    if (ts.peek().type == lexer::TokenType::OP && ts.peek().lexeme == "=") {
        if (auto var = dynamic_cast<VarRef*>(expr.get())) {
            ts.next(); // consume '='
            auto val = parse_expression();
            if (ts.peek().type == lexer::TokenType::NEWLINE) ts.next();
            return std::make_unique<AssignStmt>(var->name, std::move(val));
        }
    }
    if (ts.peek().type == lexer::TokenType::NEWLINE) ts.next();
    return std::make_unique<ExprStmt>(std::move(expr));
}

std::unique_ptr<Expr> Parser::parse_expression() {
    auto left = parse_term();
    while (ts.peek().type == lexer::TokenType::OP && (ts.peek().lexeme == "+" || ts.peek().lexeme == "-")) {
        std::string op = ts.next().lexeme;
        auto right = parse_term();
        left = std::make_unique<BinaryOp>(op, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<Expr> Parser::parse_term() {
    auto left = parse_factor();
    while (ts.peek().type == lexer::TokenType::OP && (ts.peek().lexeme == "*" || ts.peek().lexeme == "/")) {
        std::string op = ts.next().lexeme;
        auto right = parse_factor();
        left = std::make_unique<BinaryOp>(op, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<Expr> Parser::parse_factor() {
    auto t = ts.peek();
    if (t.type == lexer::TokenType::NUMBER) { ts.next(); return std::make_unique<NumberLiteral>(t.lexeme); }
    if (t.type == lexer::TokenType::STRING) { ts.next(); return std::make_unique<StringLiteral>(t.lexeme); }
    if (t.type == lexer::TokenType::IDENT) {
        ts.next();
        if (ts.peek().type == lexer::TokenType::OP && ts.peek().lexeme == "(") {
            ts.next();
            auto call = std::make_unique<CallExpr>();
            call->callee = std::make_unique<VarRef>(t.lexeme);
            call->args = parse_arglist();
            if (ts.peek().type == lexer::TokenType::OP && ts.peek().lexeme == ")") ts.next();
            return call;
        }
        return std::make_unique<VarRef>(t.lexeme);
    }
    if (t.type == lexer::TokenType::OP && t.lexeme == "(") {
        ts.next();
        auto e = parse_expression();
        if (ts.peek().type == lexer::TokenType::OP && ts.peek().lexeme == ")") ts.next();
        return e;
    }
    // Unknown token — do NOT consume it, return nullptr so callers can handle it
    return nullptr;
}

std::vector<std::unique_ptr<Expr>> Parser::parse_arglist() {
    std::vector<std::unique_ptr<Expr>> args;
    while (!ts.eof() && !(ts.peek().type == lexer::TokenType::OP && ts.peek().lexeme == ")")) {
        if (ts.peek().type == lexer::TokenType::OP && ts.peek().lexeme == ",") { ts.next(); continue; }
        args.push_back(parse_expression());
    }
    return args;
}

