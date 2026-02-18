// parser.cpp - Recursive descent parser
// TODO: Align with Python's official PEG grammar (python.gram)
// See docs/PEG_GRAMMAR_PLAN.md for implementation plan
#include "frontend/parser/parser.h"
#include "frontend/lexer/token_utils.h"
#include <iostream>

using namespace cimple;
using namespace cimple::parser;

Parser::Parser(const std::vector<lexer::Token> &tokens) : ts(tokens) {}

Module Parser::parse_module() {
  Module m;
  while (!ts.eof()) {
    auto t = ts.peek();
    // Stop at end of file
    if (t.type == lexer::TokenType::ENDMARKER)
      break;
    // Skip blank lines, indentation tokens, and comments at module level
    if (t.type == lexer::TokenType::NEWLINE ||
        t.type == lexer::TokenType::INDENT ||
        t.type == lexer::TokenType::DEDENT ||
        t.type == lexer::TokenType::COMMENT) {
      ts.next();
      continue;
    }
    auto s = parse_statement();
    if (s)
      m.body.push_back(std::move(s));
    else
      break; // genuinely unrecognized token — stop parsing
  }
  return m;
}

// ---------------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------------

std::unique_ptr<Stmt> Parser::parse_statement() {
  auto t = ts.peek();
  if (t.type == lexer::TokenType::KEYWORD && t.lexeme == "def") {
    return parse_funcdef();
  }
  if (t.type == lexer::TokenType::KEYWORD && t.lexeme == "if") {
    return parse_if();
  }
  if (t.type == lexer::TokenType::KEYWORD && t.lexeme == "while") {
    return parse_while();
  }
  if (t.type == lexer::TokenType::KEYWORD && t.lexeme == "return") {
    ts.next();
    auto val = parse_expression();
    if (ts.peek().type == lexer::TokenType::NEWLINE)
      ts.next();
    return std::make_unique<ReturnStmt>(std::move(val));
  }
  return parse_simple_statement();
}

// Parse an indented block: NEWLINE INDENT stmt* DEDENT
std::vector<std::unique_ptr<Stmt>> Parser::parse_block() {
  // Skip any trailing content on the header line up to NEWLINE
  while (!ts.eof() && ts.peek().type != lexer::TokenType::NEWLINE &&
         ts.peek().type != lexer::TokenType::INDENT)
    ts.next();
  if (ts.peek().type == lexer::TokenType::NEWLINE)
    ts.next();
  if (ts.peek().type == lexer::TokenType::INDENT)
    ts.next();

  std::vector<std::unique_ptr<Stmt>> body;
  while (!ts.eof() && ts.peek().type != lexer::TokenType::DEDENT) {
    auto stmt = parse_statement();
    if (stmt)
      body.push_back(std::move(stmt));
    else
      break;
  }
  if (ts.peek().type == lexer::TokenType::DEDENT)
    ts.next();
  return body;
}

std::unique_ptr<FuncDef> Parser::parse_funcdef() {
  ts.next(); // def
  auto nameTok = ts.next();
  if (nameTok.type != lexer::TokenType::IDENT) {
    std::cerr << "Parser error: expected function name" << std::endl;
    return nullptr;
  }
  std::string name = nameTok.lexeme;

  if (ts.peek().type == lexer::TokenType::OP && ts.peek().lexeme == "(")
    ts.next();
  std::vector<std::string> params;
  while (!ts.eof() &&
         !(ts.peek().type == lexer::TokenType::OP && ts.peek().lexeme == ")")) {
    auto tok = ts.next();
    if (tok.type == lexer::TokenType::IDENT)
      params.push_back(tok.lexeme);
    if (ts.peek().type == lexer::TokenType::OP && ts.peek().lexeme == ",")
      ts.next();
  }
  if (ts.peek().type == lexer::TokenType::OP && ts.peek().lexeme == ")")
    ts.next();

  auto fn = std::make_unique<FuncDef>();
  fn->name = name;
  fn->params = std::move(params);
  fn->body = parse_block();
  return fn;
}

// if <cond>: BLOCK [elif <cond>: BLOCK]* [else: BLOCK]
std::unique_ptr<IfStmt> Parser::parse_if() {
  auto stmt = std::make_unique<IfStmt>();

  // Parse the 'if' branch
  ts.next(); // consume 'if'
  IfBranch ifBranch;
  ifBranch.condition = parse_expression();
  // consume ':' if present
  if (ts.peek().type == lexer::TokenType::OP && ts.peek().lexeme == ":")
    ts.next();
  ifBranch.body = parse_block();
  stmt->branches.push_back(std::move(ifBranch));

  // Parse 'elif' branches
  while (!ts.eof() && ts.peek().type == lexer::TokenType::KEYWORD &&
         ts.peek().lexeme == "elif") {
    ts.next(); // consume 'elif'
    IfBranch elifBranch;
    elifBranch.condition = parse_expression();
    if (ts.peek().type == lexer::TokenType::OP && ts.peek().lexeme == ":")
      ts.next();
    elifBranch.body = parse_block();
    stmt->branches.push_back(std::move(elifBranch));
  }

  // Parse optional 'else' branch
  if (!ts.eof() && ts.peek().type == lexer::TokenType::KEYWORD &&
      ts.peek().lexeme == "else") {
    ts.next(); // consume 'else'
    if (ts.peek().type == lexer::TokenType::OP && ts.peek().lexeme == ":")
      ts.next();
    IfBranch elseBranch;
    elseBranch.condition = nullptr; // else has no condition
    elseBranch.body = parse_block();
    stmt->branches.push_back(std::move(elseBranch));
  }

  return stmt;
}

// while <cond>: BLOCK
std::unique_ptr<WhileStmt> Parser::parse_while() {
  ts.next(); // consume 'while'
  auto stmt = std::make_unique<WhileStmt>();
  stmt->condition = parse_expression();
  if (ts.peek().type == lexer::TokenType::OP && ts.peek().lexeme == ":")
    ts.next();
  stmt->body = parse_block();
  return stmt;
}

std::unique_ptr<Stmt> Parser::parse_simple_statement() {
  auto t = ts.peek();
  if (t.type == lexer::TokenType::NEWLINE) {
    ts.next();
    return nullptr;
  }
  if (t.type == lexer::TokenType::ENDMARKER)
    return nullptr;
  // Skip bare INDENT/DEDENT tokens at statement level
  if (t.type == lexer::TokenType::INDENT ||
      t.type == lexer::TokenType::DEDENT) {
    ts.next();
    return nullptr;
  }
  // Skip comment tokens (safety net — lexer should strip these, but just in
  // case)
  if (t.type == lexer::TokenType::COMMENT) {
    ts.next();
    return nullptr;
  }

  auto expr = parse_expression();
  if (!expr)
    return nullptr;
  if (ts.peek().type == lexer::TokenType::OP && ts.peek().lexeme == "=") {
    if (auto var = dynamic_cast<VarRef *>(expr.get())) {
      ts.next(); // consume '='
      auto val = parse_expression();
      if (ts.peek().type == lexer::TokenType::NEWLINE)
        ts.next();
      return std::make_unique<AssignStmt>(var->name, std::move(val));
    }
  }
  if (ts.peek().type == lexer::TokenType::NEWLINE)
    ts.next();
  return std::make_unique<ExprStmt>(std::move(expr));
}

// ---------------------------------------------------------------------------
// Expressions (precedence low → high)
// expression → comparison
// comparison → additive (( '==' | '!=' | '<' | '>' | '<=' | '>=' ) additive)*
// additive   → term (( '+' | '-' ) term)*
// term       → unary (( '*' | '/' ) unary)*
// unary      → 'not' unary | '-' unary | factor
// factor     → NUMBER | STRING | 'True' | 'False' | IDENT ['(' arglist ')'] |
// '(' expression ')'
// ---------------------------------------------------------------------------

std::unique_ptr<Expr> Parser::parse_expression() { return parse_comparison(); }

std::unique_ptr<Expr> Parser::parse_comparison() {
  auto left = parse_additive();
  static const std::vector<std::string> cmp_ops = {"==", "!=", "<",
                                                   ">",  "<=", ">="};
  while (ts.peek().type == lexer::TokenType::OP) {
    const std::string &op = ts.peek().lexeme;
    bool is_cmp = false;
    for (auto &c : cmp_ops)
      if (c == op) {
        is_cmp = true;
        break;
      }
    if (!is_cmp)
      break;
    ts.next();
    auto right = parse_additive();
    left = std::make_unique<BinaryOp>(op, std::move(left), std::move(right));
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_additive() {
  auto left = parse_term();
  while (ts.peek().type == lexer::TokenType::OP &&
         (ts.peek().lexeme == "+" || ts.peek().lexeme == "-")) {
    std::string op = ts.next().lexeme;
    auto right = parse_term();
    left = std::make_unique<BinaryOp>(op, std::move(left), std::move(right));
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_term() {
  auto left = parse_unary();
  while (ts.peek().type == lexer::TokenType::OP &&
         (ts.peek().lexeme == "*" || ts.peek().lexeme == "/")) {
    std::string op = ts.next().lexeme;
    auto right = parse_unary();
    left = std::make_unique<BinaryOp>(op, std::move(left), std::move(right));
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_unary() {
  auto t = ts.peek();
  // 'not' has lower precedence than comparisons: not (x < y)
  if (t.type == lexer::TokenType::KEYWORD && t.lexeme == "not") {
    ts.next();
    auto operand = parse_comparison(); // not binds looser than comparisons
    return std::make_unique<UnaryOp>("not", std::move(operand));
  }
  // unary minus recurses into itself for chaining: --x
  if (t.type == lexer::TokenType::OP && t.lexeme == "-") {
    ts.next();
    auto operand = parse_unary();
    return std::make_unique<UnaryOp>("-", std::move(operand));
  }
  return parse_factor();
}

std::unique_ptr<Expr> Parser::parse_factor() {
  auto t = ts.peek();
  if (t.type == lexer::TokenType::NUMBER) {
    ts.next();
    return std::make_unique<NumberLiteral>(t.lexeme);
  }
  if (t.type == lexer::TokenType::STRING) {
    ts.next();
    return std::make_unique<StringLiteral>(t.lexeme);
  }
  // Boolean literals
  if (t.type == lexer::TokenType::KEYWORD && t.lexeme == "True") {
    ts.next();
    return std::make_unique<BoolLiteral>(true);
  }
  if (t.type == lexer::TokenType::KEYWORD && t.lexeme == "False") {
    ts.next();
    return std::make_unique<BoolLiteral>(false);
  }
  if (t.type == lexer::TokenType::IDENT) {
    ts.next();
    if (ts.peek().type == lexer::TokenType::OP && ts.peek().lexeme == "(") {
      ts.next();
      auto call = std::make_unique<CallExpr>();
      call->callee = std::make_unique<VarRef>(t.lexeme);
      call->args = parse_arglist();
      if (ts.peek().type == lexer::TokenType::OP && ts.peek().lexeme == ")")
        ts.next();
      return call;
    }
    return std::make_unique<VarRef>(t.lexeme);
  }
  if (t.type == lexer::TokenType::OP && t.lexeme == "(") {
    ts.next();
    auto e = parse_expression();
    if (ts.peek().type == lexer::TokenType::OP && ts.peek().lexeme == ")")
      ts.next();
    return e;
  }
  // Unknown token — do NOT consume it, return nullptr so callers can handle it
  return nullptr;
}

std::vector<std::unique_ptr<Expr>> Parser::parse_arglist() {
  std::vector<std::unique_ptr<Expr>> args;
  while (!ts.eof() &&
         !(ts.peek().type == lexer::TokenType::OP && ts.peek().lexeme == ")")) {
    if (ts.peek().type == lexer::TokenType::OP && ts.peek().lexeme == ",") {
      ts.next();
      continue;
    }
    auto arg = parse_expression();
    if (!arg)
      break; // safety: unknown token in arg list
    args.push_back(std::move(arg));
  }
  return args;
}
