#pragma once

#include "../lexer/lexer.h"
#include "../token_stream.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>


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
  NumberLiteral(std::string v) : value(std::move(v)) {}
  std::string to_string() const override { return "Number(" + value + ")"; }
};

struct StringLiteral : Expr {
  std::string value;
  StringLiteral(std::string v) : value(std::move(v)) {}
  std::string to_string() const override { return "String(" + value + ")"; }
};

struct BoolLiteral : Expr {
  bool value;
  BoolLiteral(bool v) : value(v) {}
  std::string to_string() const override {
    return value ? "Bool(True)" : "Bool(False)";
  }
};

struct VarRef : Expr {
  std::string name;
  VarRef(std::string n) : name(std::move(n)) {}
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

struct UnaryOp : Expr {
  std::string op; // "not", "-"
  std::unique_ptr<Expr> operand;
  UnaryOp(std::string o, std::unique_ptr<Expr> e)
      : op(std::move(o)), operand(std::move(e)) {}
  std::string to_string() const override { return "UnaryOp(" + op + ")"; }
};

// Statements
struct Stmt : Node {};

struct ExprStmt : Stmt {
  std::unique_ptr<Expr> expr;
  ExprStmt(std::unique_ptr<Expr> e) : expr(std::move(e)) {}
  std::string to_string() const override { return "ExprStmt"; }
};

struct AssignStmt : Stmt {
  std::string target;
  std::unique_ptr<Expr> value;
  AssignStmt(std::string t, std::unique_ptr<Expr> v)
      : target(std::move(t)), value(std::move(v)) {}
  std::string to_string() const override {
    return "AssignStmt(" + target + ")";
  }
};

struct ReturnStmt : Stmt {
  std::unique_ptr<Expr> value;
  ReturnStmt(std::unique_ptr<Expr> v) : value(std::move(v)) {}
  std::string to_string() const override { return "ReturnStmt"; }
};

struct FuncDef : Stmt {
  std::string name;
  std::vector<std::string> params;
  std::vector<std::unique_ptr<Stmt>> body;
  std::string to_string() const override { return "FuncDef(" + name + ")"; }
};

// if / elif / else
struct IfBranch {
  std::unique_ptr<Expr> condition; // nullptr for else branch
  std::vector<std::unique_ptr<Stmt>> body;
};

struct IfStmt : Stmt {
  std::vector<IfBranch> branches; // branches[0] = if, [1..n-1] = elif, last may
                                  // be else (condition==nullptr)
  std::string to_string() const override { return "IfStmt"; }
};

// while loop
struct WhileStmt : Stmt {
  std::unique_ptr<Expr> condition;
  std::vector<std::unique_ptr<Stmt>> body;
  std::string to_string() const override { return "WhileStmt"; }
};

struct Module {
  std::vector<std::unique_ptr<Stmt>> body;
};

class Parser {
public:
  Parser(const std::vector<lexer::Token> &tokens);

  Module parse_module();

private:
  TokenStream ts;

  std::unique_ptr<Stmt> parse_statement();
  std::unique_ptr<Stmt> parse_simple_statement();
  std::unique_ptr<FuncDef> parse_funcdef();
  std::unique_ptr<IfStmt> parse_if();
  std::unique_ptr<WhileStmt> parse_while();

  // Parse an indented block of statements (after NEWLINE + INDENT)
  std::vector<std::unique_ptr<Stmt>> parse_block();

  // expressions (precedence: comparison > arithmetic > term > factor)
  std::unique_ptr<Expr> parse_expression(); // handles 'or'/'and' (future)
  std::unique_ptr<Expr> parse_comparison(); // handles ==, !=, <, >, <=, >=
  std::unique_ptr<Expr> parse_additive();   // handles + and -
  std::unique_ptr<Expr> parse_term();       // handles * and /
  std::unique_ptr<Expr> parse_unary();      // handles 'not' and unary '-'
  std::unique_ptr<Expr>
  parse_factor(); // handles literals, identifiers, calls, parens
  std::vector<std::unique_ptr<Expr>> parse_arglist();
};

} // namespace parser
} // namespace cimple
