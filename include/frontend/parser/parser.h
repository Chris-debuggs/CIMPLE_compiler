#pragma once

#include "../token_stream.h"
#include "../lexer/lexer.h"
#include <memory>
#include <string>
#include <vector>
#include <optional>

namespace cimple {
namespace parser {

// Richer AST node hierarchy
struct Node {
    virtual ~Node() = default;
    virtual std::string to_string() const = 0;
};

struct Expr : Node {};

struct NumberLiteral : Expr {
    std::string value;
    NumberLiteral(std::string v): value(std::move(v)) {}
    std::string to_string() const override { return "Number(" + value + ")"; }
};

struct StringLiteral : Expr {
    std::string value;
    StringLiteral(std::string v): value(std::move(v)) {}
    std::string to_string() const override { return "String(" + value + ")"; }
};

struct VarRef : Expr {
    std::string name;
    VarRef(std::string n): name(std::move(n)) {}
    std::string to_string() const override { return "Var(" + name + ")"; }
};

struct CallExpr : Expr {
    std::unique_ptr<Expr> callee;
    std::vector<std::unique_ptr<Expr>> args;
    std::string to_string() const override { return "Call(...)"; }
};

struct BinaryOp : Expr {
    std::string op;
    std::unique_ptr<Expr> left, right;
    BinaryOp(std::string o, std::unique_ptr<Expr> l, std::unique_ptr<Expr> r)
        : op(std::move(o)), left(std::move(l)), right(std::move(r)) {}
    std::string to_string() const override { return "BinOp(" + op + ")"; }
};

// Statements
struct Stmt : Node {};

struct ExprStmt : Stmt {
    std::unique_ptr<Expr> expr;
    ExprStmt(std::unique_ptr<Expr> e): expr(std::move(e)) {}
    std::string to_string() const override { return "ExprStmt"; }
};

struct AssignStmt : Stmt {
    std::string target;
    std::unique_ptr<Expr> value;
    AssignStmt(std::string t, std::unique_ptr<Expr> v): target(std::move(t)), value(std::move(v)) {}
    std::string to_string() const override { return "AssignStmt(" + target + ")"; }
};

struct ReturnStmt : Stmt {
    std::unique_ptr<Expr> value;
    ReturnStmt(std::unique_ptr<Expr> v): value(std::move(v)) {}
    std::string to_string() const override { return "ReturnStmt"; }
};

struct FuncDef : Stmt {
    std::string name;
    std::vector<std::string> params;
    std::vector<std::unique_ptr<Stmt>> body;
    std::string to_string() const override { return "FuncDef(" + name + ")"; }
};

struct Module {
    std::vector<std::unique_ptr<Stmt>> body;
};

class Parser {
public:
    Parser(const std::vector<lexer::Token>& tokens);

    Module parse_module();

private:
    TokenStream ts;

    std::unique_ptr<Stmt> parse_statement();
    std::unique_ptr<Stmt> parse_simple_statement();
    std::unique_ptr<FuncDef> parse_funcdef();

    // expressions
    std::unique_ptr<Expr> parse_expression();
    std::unique_ptr<Expr> parse_term();
    std::unique_ptr<Expr> parse_factor();
    std::vector<std::unique_ptr<Expr>> parse_arglist();
};

} // namespace parser
} // namespace cimple
