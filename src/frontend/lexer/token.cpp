// token.cpp - token utility implementations
#include "frontend/lexer/token_utils.h"
#include <sstream>

using namespace cimple::lexer;

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

std::string cimple::lexer::token_to_string(const Token& tok) {
	std::ostringstream ss;
	ss << "(" << token_type_to_string(tok.type) << ", '" << tok.lexeme << "', line=" << tok.line << ", col=" << tok.column << ")";
	return ss.str();
}

