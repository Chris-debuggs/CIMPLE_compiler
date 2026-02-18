// lexer.cpp - basic lexer for the same syntax as Python
// Optimized with std::string_view for zero-copy performance
#include "frontend/lexer/lexer.h"
#include <cctype>
#include <sstream>
#include <string_view>
#include <unordered_set>

using namespace cimple::lexer;

// Use unordered_set for O(1) keyword lookup with string_view
static const std::unordered_set<std::string_view> keywords = {
	"def","return","if","elif","else","for","while","in",
	"import","from","as","pass","break","continue","class",
	"and","or","not","True","False","None"
};

static bool is_keyword(std::string_view s) {
	return keywords.find(s) != keywords.end();
}

std::vector<Token> cimple::lexer::lex(const std::string& source) {
	return lex_from_view(source);
}

std::vector<Token> cimple::lexer::lex_from_view(std::string_view source) {
	std::vector<Token> out;
	std::vector<int> indent_stack = {0};
	int lineno = 0;
	
	// Process line by line using string_view to avoid copies
	size_t line_start = 0;
	while (line_start < source.length()) {
		++lineno;
		
		// Find end of line
		size_t line_end = source.find('\n', line_start);
		if (line_end == std::string_view::npos) {
			line_end = source.length();
		}
		
		std::string_view line = source.substr(line_start, line_end - line_start);
		line_start = line_end + 1;
		
		// Convert tabs to spaces (we need a string for this transformation)
		std::string tline;
		tline.reserve(line.length() * 4); // Reserve space for tab expansion
		for (char c : line) {
			if (c == '\t') {
				tline.append(4, ' ');
			} else {
				tline.push_back(c);
			}
		}
		
		// Now work with tline as string_view for the rest
		std::string_view tline_view = tline;
		
		// Count leading spaces
		size_t pos = 0;
		while (pos < tline_view.length() && tline_view[pos] == ' ') ++pos;
		
		// Skip empty or comment-only lines
		size_t first_non_ws = tline_view.find_first_not_of(' ');
		if (first_non_ws == std::string_view::npos) continue;
		if (tline_view[first_non_ws] == '#') continue;
		
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
		
		// Tokenize the rest of the line using string_view
		size_t i = pos;
		int col = static_cast<int>(pos) + 1;
		while (i < tline_view.length()) {
			char c = tline_view[i];
			if (c == ' ' || c == '\r' || c == '\n') { ++i; ++col; continue; }
			if (c == '#') {
				// comment: consume rest (convert to string only when storing)
				std::string_view comment_view = tline_view.substr(i);
				out.push_back({TokenType::COMMENT, std::string(comment_view), {lineno, col}});
				break;
			}
			
			if (std::isalpha((unsigned char)c) || c == '_') {
				size_t j = i + 1;
				while (j < tline_view.length() && (std::isalnum((unsigned char)tline_view[j]) || tline_view[j] == '_')) ++j;
				std::string_view ident_view = tline_view.substr(i, j - i);
				TokenType tt = is_keyword(ident_view) ? TokenType::KEYWORD : TokenType::IDENT;
				// Convert to string only when storing in Token
				out.push_back({tt, std::string(ident_view), {lineno, col}});
				col += static_cast<int>(j - i);
				i = j;
				continue;
			}
			
			if (std::isdigit((unsigned char)c)) {
				size_t j = i + 1;
				bool has_dot = false;
				while (j < tline_view.length() && (std::isdigit((unsigned char)tline_view[j]) || (!has_dot && tline_view[j] == '.'))) {
					if (tline_view[j] == '.') has_dot = true;
					++j;
				}
				std::string_view num_view = tline_view.substr(i, j - i);
				out.push_back({TokenType::NUMBER, std::string(num_view), {lineno, col}});
				col += static_cast<int>(j - i);
				i = j;
				continue;
			}
			
			if (c == '"' || c == '\'') {
				char quote = c;
				size_t j = i + 1;
				std::string buf;
				buf.push_back(quote);
				while (j < tline_view.length()) {
					char cc = tline_view[j];
					buf.push_back(cc);
					if (cc == quote) { ++j; break; }
					if (cc == '\\' && j + 1 < tline_view.length()) {
						buf.push_back(tline_view[j + 1]);
						j += 2;
						continue;
					}
					++j;
				}
				out.push_back({TokenType::STRING, buf, {lineno, col}});
				col += static_cast<int>(j - i);
				i = j;
				continue;
			}
			
			// operators and punctuation
			std::string_view op_view = tline_view.substr(i, 1);
			// support two-char ops
			if (i + 1 < tline_view.length()) {
				std::string_view two_view = tline_view.substr(i, 2);
				static const std::vector<std::string_view> two_ops = {
					"==", "!=", "<=", ">=", "+=", "-=", "*=", "/=", "//", "**", "->", "::", "<<", ">>"
				};
				for (auto& o : two_ops) {
					if (o == two_view) {
						out.push_back({TokenType::OP, std::string(two_view), {lineno, col}});
						i += 2;
						col += 2;
						goto next_char;
					}
				}
			}
			out.push_back({TokenType::OP, std::string(op_view), {lineno, col}});
			++i;
			++col;
		next_char:;
		}
		
		// end of logical line
		out.push_back({TokenType::NEWLINE, "", {lineno, static_cast<int>(tline_view.length()) + 1}});
	}
	
	// close remaining indents
	while (indent_stack.size() > 1) {
		indent_stack.pop_back();
		out.push_back({TokenType::DEDENT, "", {static_cast<int>(indent_stack.size()), 1}});
	}
	
	out.push_back({TokenType::ENDMARKER, "", {lineno + 1, 1}});
	return out;
}

// Token helper functions (defined here since token.cpp is not included in the build)
std::string cimple::lexer::token_type_to_string(TokenType t) {
	switch (t) {
		case TokenType::INDENT:    return "INDENT";
		case TokenType::DEDENT:    return "DEDENT";
		case TokenType::NEWLINE:   return "NEWLINE";
		case TokenType::ENDMARKER: return "ENDMARKER";
		case TokenType::IDENT:     return "IDENT";
		case TokenType::NUMBER:    return "NUMBER";
		case TokenType::STRING:    return "STRING";
		case TokenType::OP:        return "OP";
		case TokenType::KEYWORD:   return "KEYWORD";
		case TokenType::COMMENT:   return "COMMENT";
	}
	return "<unknown>";
}

std::string cimple::lexer::token_to_string(const Token& tok) {
	std::ostringstream ss;
	ss << token_type_to_string(tok.type) << " ";
	if (!tok.lexeme.empty()) ss << "('" << tok.lexeme << "') ";
	ss << "@" << tok.loc.line << ":" << tok.loc.column;
	return ss.str();
}
