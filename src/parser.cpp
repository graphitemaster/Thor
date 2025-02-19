#include <stdio.h>

#include "parser.h"

namespace Thor {

struct Debug {
	Debug(const char* name, int line) {
		for (int i = 0; i < s_depth; i++) {
			printf("  ");
		}
		printf("%s:%d\n", name, line);
		s_depth++;
	}
	~Debug() {
		s_depth--;
	}
	static inline int s_depth = 0;
};

// #define TRACE()
#define TRACE() \
	auto debug_ ## __LINE__ = Debug{__func__, __LINE__}

Maybe<Parser> Parser::open(System& sys, StringView file) {
	if (auto lexer = Lexer::open(sys, file)) {
		return Parser { sys, move(*lexer) };
	}
	return {};
}

Parser::Parser(System& sys, Lexer&& lexer)
	: sys_{sys}
	, temporary_{sys.allocator}
	, ast_{sys.allocator}
	, lexer_{move(lexer)}
	, token_{lexer_.next()}
{
}

StringView Parser::parse_ident() {
	const auto result = lexer_.string(token_);
	eat(); // Eat <ident>
	return result;
}

Maybe<Array<AstRef<AstExpr>>> Parser::parse_expr_list(Bool lhs) {
	TRACE();
	Array<AstRef<AstExpr>> exprs{sys_.allocator};
	for (;;) {
		auto expr = parse_expr(lhs);
		if (!expr || !exprs.push_back(expr)) {
			return {};
		}
		if (!is_kind(TokenKind::COMMA) || is_kind(TokenKind::ENDOF)) {
			break;
		}
	}
	return exprs;
}

AstRef<AstProcExpr> Parser::parse_proc_expr() {
	eat(); // Eat 'proc'
	Maybe<Array<AstRef<AstDeclStmt>>> params{sys_.allocator};
	for (;;) {
		if(is_operator(OperatorKind::RPAREN)) break;
		eat();
	}
	eat(); // Eat ')'
	eat(); // Eat '-'
	eat(); // Eat '>'
	auto ret = parse_type_expr();
	auto body = parse_block_stmt();
	return ast_.create<AstProcExpr>(move(params), body, ret);
}

AstRef<AstStmt> Parser::parse_simple_stmt() {
	TRACE();
	auto lhs = parse_expr_list(true);
	if (is_operator(OperatorKind::COLON)) {
		eat(); // Eat ':'
		auto type = parse_type_expr();
		Maybe<Array<AstRef<AstExpr>>> values;
		if (is_operator(OperatorKind::EQ) ||
		    is_operator(OperatorKind::COLON))
		{
			eat(); // Eat ':' or '='
			values = parse_expr_list(false);
		}
		return ast_.create<AstDeclStmt>(move(*lhs), type, move(values));
	} else if (is_assignment()) {
		Token token = token_;
		eat();
		auto rhs = parse_expr_list(false);
		if (!rhs || rhs->length() == 0) {
			error("No right-hand side assignment statement");
			return {};
		}
		if (!lhs) {
			error("No left-hand side assignments");
			return {};
		}
		return ast_.create<AstAssignStmt>(move(*lhs), token, move(*rhs));

	}
	if (!lhs || lhs->length() > 1) {
		error("Expected 1 expression");
		return {};
	}

	return ast_.create<AstExprStmt>((*lhs)[0]);
}

AstRef<AstStmt> Parser::parse_stmt() {
	TRACE();
	AstRef<AstStmt> stmt;
	if (is_kind(TokenKind::SEMICOLON)) {
		return parse_empty_stmt();
	} else if (is_kind(TokenKind::LBRACE)) {
		return parse_block_stmt();
	} else if (is_kind(TokenKind::ATTRIBUTE)) {
		// TODO
	} else if (is_kind(TokenKind::DIRECTIVE)) {
		// TODO
	} else if (is_keyword(KeywordKind::PACKAGE)) {
		stmt = parse_package_stmt();
	} else if (is_keyword(KeywordKind::IMPORT)) {
		stmt = parse_import_stmt();
	} else if (is_keyword(KeywordKind::DEFER)) {
		stmt = parse_defer_stmt();
	} else if (is_keyword(KeywordKind::RETURN)) {
		// TODO
	} else if (is_keyword(KeywordKind::BREAK)) {
		stmt = parse_break_stmt();
	} else if (is_keyword(KeywordKind::CONTINUE)) {
		stmt = parse_continue_stmt();
	} else if (is_keyword(KeywordKind::FALLTHROUGH)) {
		stmt = parse_fallthrough_stmt();
	} else if (is_keyword(KeywordKind::FOREIGN)) {
		// TODO
	} else if (is_keyword(KeywordKind::IF)) {
		stmt = parse_if_stmt();
	} else if (is_keyword(KeywordKind::WHEN)) {
		// TODO
	} else if (is_keyword(KeywordKind::FOR)) {
		// TODO
	} else if (is_keyword(KeywordKind::SWITCH)) {
		// TODO
	} else if (is_keyword(KeywordKind::USING)) {
		// TODO
	} else {
		stmt = parse_simple_stmt();
	}
	if (!stmt) {
		return {};
	}
	if (is_kind(TokenKind::SEMICOLON)) {
		eat(); // Eat ';'
		return stmt;
	}
	if (!is_kind(TokenKind::ENDOF)) {
		error("Expected ';' or newline after statement");
	}
	return {};
}

AstRef<AstEmptyStmt> Parser::parse_empty_stmt() {
	TRACE();
	if (!is_kind(TokenKind::SEMICOLON)) {
		error("Expected ';' (or newline)");
		return {};
	}
	eat(); // Eat ';'
	return ast_.create<AstEmptyStmt>();
}

AstRef<AstBlockStmt> Parser::parse_block_stmt() {
	TRACE();
	if (!is_kind(TokenKind::LBRACE)) {
		error("Expected '{'");
		return {};
	}
	eat(); // Eat '{'
	Array<AstRef<AstStmt>> stmts{sys_.allocator};
	while (!is_kind(TokenKind::RBRACE) && !is_kind(TokenKind::ENDOF)) {
		auto stmt = parse_stmt();
		if (!stmt || !stmts.push_back(stmt)) {
			return {};
		}
	}
	if (!is_kind(TokenKind::RBRACE)) {
		error("Expected '}'");
		return {};
	}
	eat(); // Eat '}'
	return ast_.create<AstBlockStmt>(move(stmts));
}

AstRef<AstPackageStmt> Parser::parse_package_stmt() {
	TRACE();
	eat(); // Eat 'package'
	if (is_kind(TokenKind::IDENTIFIER)) {
		const auto ident = lexer_.string(token_);
		eat(); // Eat <ident>
		if (auto ref = ast_.insert(ident)) {
			return ast_.create<AstPackageStmt>(ref);
		} else {
			// Out of memory.
			return {};
		}
	}
	error("Expected identifier for package");
	return {};
}

AstRef<AstImportStmt> Parser::parse_import_stmt() {
	TRACE();
	eat(); // Eat 'import'
	if (is_literal(LiteralKind::STRING)) {
		const auto path = lexer_.string(token_);
		eat(); // Eat ""
		if (auto ref = ast_.insert(path)) {
			return ast_.create<AstImportStmt>(ref);
		} else {
			return {};
		}
	}
	error("Expected string literal for import path");
	return {};
}

AstRef<AstBreakStmt> Parser::parse_break_stmt() {
	TRACE();
	if (!is_keyword(KeywordKind::BREAK)) {
		error("Expected 'break'");
		return {};
	}
	eat(); // Eat 'break'
	StringRef label;
	if (is_kind(TokenKind::IDENTIFIER)) {
		label = ast_.insert(parse_ident());
	}
	return ast_.create<AstBreakStmt>(label);
}

AstRef<AstContinueStmt> Parser::parse_continue_stmt() {
	TRACE();
	if (!is_keyword(KeywordKind::CONTINUE)) {
		error("Expected 'continue'");
		return {};
	}
	eat(); // Eat 'continue'
	StringRef label;
	if (is_kind(TokenKind::IDENTIFIER)) {
		label = ast_.insert(parse_ident());
	}
	return ast_.create<AstContinueStmt>(label);
}

AstRef<AstFallthroughStmt> Parser::parse_fallthrough_stmt() {
	TRACE();
	if (!is_keyword(KeywordKind::FALLTHROUGH)) {
		error("Expected 'fallthrough'");
		return {};
	}
	eat(); // Eat 'fallthrough'
	return ast_.create<AstFallthroughStmt>();
}

AstRef<AstIfStmt> Parser::parse_if_stmt() {
	TRACE();
	if (!is_keyword(KeywordKind::IF)) {
		error("Expected 'if'");
		return {};
	}
	eat(); // Eat 'if'
	Maybe<AstRef<AstStmt>> init;
	AstRef<AstExpr>        cond;
	AstRef<AstStmt>        on_true;
	Maybe<AstRef<AstStmt>> on_false;

	auto prev_level = this->expr_level_;
	this->expr_level_ = -1;

	auto prev_allow_in_expr = this->allow_in_expr_;
	this->allow_in_expr_ = true;

	if (is_kind(TokenKind::SEMICOLON)) {
		cond = parse_expr(false);
	} else {
		init = parse_simple_stmt();
		if (is_kind(TokenKind::SEMICOLON)) {
			cond = parse_expr(false);
		} else {
			if (init && ast_[*init].is_stmt<AstExprStmt>()) {
				cond = static_cast<AstExprStmt const &>(ast_[*init]).expr;
			} else {
				error("Expected a boolean expression");
			}
			init.reset();
		}
	}

	this->expr_level_    = prev_level;
	this->allow_in_expr_ = prev_allow_in_expr;

	if (!cond.is_valid()) {
		error("Expected a condition for if statement");
	}

	if (is_keyword(KeywordKind::DO)) {
		eat(); // Eat 'do'
		// TODO(bill): enforce same line behaviour
		on_true = parse_stmt();
	} else {
		on_true = parse_block_stmt();
	}

	skip_possible_newline_for_literal();
	if (is_keyword(KeywordKind::ELSE)) {
		// Token else_tok = token_;
		eat();
		if (is_keyword(KeywordKind::IF)) {
			on_false = parse_if_stmt();
		} else if (is_kind(TokenKind::LBRACE)) {
			on_false = parse_block_stmt();
		} else if (is_keyword(KeywordKind::DO)) {
			eat(); // Eat 'do'
			// TODO(bill): enforce same line behaviour
			on_false = parse_stmt();
		} else {
			error("Expected if statement block statement");
		}
	}

	return ast_.create<AstIfStmt>(move(init), cond, on_true, move(on_false));
}


Bool Parser::skip_possible_newline_for_literal() {
	if (is_kind(TokenKind::SEMICOLON) && lexer_.string(token_) == "\n") {
		// peek
		eat();
		// TODO
	}
	return false;
}


AstRef<AstDeferStmt> Parser::parse_defer_stmt() {
	TRACE();
	if (!is_keyword(KeywordKind::DEFER)) {
		error("Expected 'defer'");
		return {};
	}
	eat(); // Eat 'defer'
	auto stmt = parse_stmt();
	if (!stmt) {
		return {};
	}
	auto& s = ast_[stmt];
	if (s.is_stmt<AstEmptyStmt>()) {
		error("Empty statement after defer (e.g. ';')");
		return {};
	} else if (s.is_stmt<AstDeferStmt>()) {
		error("Cannot defer a defer statement");
		return {};
	}
	return ast_.create<AstDeferStmt>(stmt);
}

AstRef<AstIdentExpr> Parser::parse_ident_expr() {
	TRACE();
	if (!is_kind(TokenKind::IDENTIFIER)) {
		error("Expected identifier");
		return {};
	}
	const auto ident = ast_.insert(parse_ident());
	return ast_.create<AstIdentExpr>(ident);
}

AstRef<AstUndefExpr> Parser::parse_undef_expr() {
	TRACE();
	if (!is_kind(TokenKind::UNDEFINED)) {
		error("Expected '---'");
		return {};
	}
	eat(); // Eat '---'
	return ast_.create<AstUndefExpr>();
}

AstRef<AstContextExpr> Parser::parse_context_expr() {
	TRACE();
	if (!is_keyword(KeywordKind::CONTEXT)) {
		error("Expected 'context'");
		return {};
	}
	eat(); // Eat 'context'
	return ast_.create<AstContextExpr>();
}

AstRef<AstExpr> Parser::parse_expr(Bool lhs) {
	TRACE();
	return parse_bin_expr(lhs, 1);
}

AstRef<AstExpr> Parser::parse_bin_expr(Bool lhs, Uint32 prec) {
	TRACE();
	static constexpr const Uint32 PREC[] = {
		#define OPERATOR(ENUM, NAME, MATCH, PREC, NAMED, ASI) PREC,
		#include "lexer.inl"
	};
	auto expr = parse_unary_expr(lhs);
	for (;;) {
		if (!is_kind(TokenKind::OPERATOR)) {
			// error("Expected operator in binary expression");
			break;
		}
		if (PREC[Uint32(token_.as_operator)] < prec) {
			// Stop climbing, found the correct precedence.
			break;
		}
		if (is_operator(OperatorKind::QUESTION)) {
			auto on_true = parse_expr(lhs);
			if (!is_operator(OperatorKind::COLON)) {
				error("Expected ':' after ternary condition");
				return {};
			}
			eat(); // Eat ':'
			auto on_false = parse_expr(lhs);
			expr = ast_.create<AstTernaryExpr>(expr, on_true, on_false);
		} else {
			auto rhs = parse_bin_expr(false, prec+1);
			if (!rhs) {
				error("Expected expression on right-hand side of binary operator");
				return {};
			}
			expr = ast_.create<AstBinExpr>(expr, rhs, token_.as_operator);
		}
		lhs = false;
	}
	return expr;
}

AstRef<AstTypeExpr> Parser::parse_type_expr() {
	TRACE();
	auto operand = parse_operand(true);
	auto atom = parse_unary_atom(operand, true);
	return ast_.create<AstTypeExpr>(atom);
}

AstRef<AstExpr> Parser::parse_unary_atom(AstRef<AstExpr> operand, Bool lhs) {
	TRACE();
	if (!operand) {
		// printf("No operand\n");
		// error("Expected an operand");
		return {};
	}
	for (;;) {
		if (is_operator(OperatorKind::LPAREN)) {
			// operand(...)
		} else if (is_operator(OperatorKind::PERIOD)) {
			// operand.expr(...)
		} else if (is_operator(OperatorKind::ARROW)) {
			// operand->expr()
		} else if (is_operator(OperatorKind::LBRACKET)) {
			// operand[a]
			// operand[:]
			// operand[a:]
			// operand[:a]
			// operand[a:b]
			// operand[a,b]
			// operand[a..=b]
			// operand[a..<b]
			// operand[...]
			// operand[?]
		} else if (is_operator(OperatorKind::POINTER)) {
			// operand^
		} else if (is_operator(OperatorKind::OR_RETURN)) {
			// operand or_return
		} else if (is_operator(OperatorKind::OR_BREAK)) {
			// operand or_break
		} else if (is_operator(OperatorKind::OR_CONTINUE)) {
			// operand or_continue
		} else if (is_kind(TokenKind::LBRACE)) {
			// operand {
			if (lhs) {
				break;
			}
		} else {
			break;
		}
		lhs = false;
	}
	return operand;
}

AstRef<AstExpr> Parser::parse_unary_expr(Bool lhs) {
	TRACE();
	if (is_operator(OperatorKind::TRANSMUTE) ||
	    is_operator(OperatorKind::CAST))
	{
		eat(); // Eat 'transmute' or 'cast'
		if (!is_operator(OperatorKind::LPAREN)) {
			error("Expected '(' after cast");
			return {};
		}
		eat(); // Eat '('
		// auto type = parse_type();
		if (!is_operator(OperatorKind::RPAREN)) {
			error("Expected ')' after cast");
			return {};
		}
		eat(); // Eat ')'
		if (auto expr = parse_unary_expr(lhs)) {
			// return ast_.create<AstExplicitCastExpr>(type, expr);
		}
	} else if (is_operator(OperatorKind::AUTO_CAST)) {
		eat(); // Eat 'auto_cast'
		if (auto expr = parse_unary_expr(lhs)) {
			// return ast_.create<AstAutoCastExpr>(expr);
		}
	} else if (is_operator(OperatorKind::ADD)  ||
	           is_operator(OperatorKind::SUB)  ||
	           is_operator(OperatorKind::XOR)  ||
	           is_operator(OperatorKind::BAND) ||
	           is_operator(OperatorKind::LNOT) ||
	           is_operator(OperatorKind::MUL))
	{
		const auto op = token_.as_operator;
		eat(); // Eat op
		if (auto expr = parse_unary_expr(lhs)) {
			return ast_.create<AstUnaryExpr>(expr, op);
		}
	} else if (is_operator(OperatorKind::PERIOD)) {
		eat(); // Eat '.'
		// if (auto ident = parse_ident()) {
		// 	//return ast_.create<AstImplicitSelectorExpr>(ident);
		// }
	}
	auto operand = parse_operand(lhs);
	return parse_unary_atom(operand, lhs);
}

AstRef<AstStructExpr> Parser::parse_struct_expr() {
	TRACE();
	if (!is_keyword(KeywordKind::STRUCT)) {
		error("Expected 'struct'");
		return {};
	}
	eat(); // Eat 'struct'
	if (is_operator(OperatorKind::LPAREN)) {
		// Parametric polymorphic struct
		eat(); // Eat '('
		// TODO
		eat(); // Eat ')'
	}
	// Array<AstDirective> directives{sys.allocator};
	while (is_kind(TokenKind::DIRECTIVE)) {
		// auto directive = parse_directive();
		// if (!directive || !directives.push_back(directive)) {
		// 	return {};
		// }
	}
	if (!is_kind(TokenKind::LBRACE)) {
		error("Expected '{");
		return {};
	}
	eat(); // Eat '{'
	Array<AstRef<AstDeclStmt>> fields{sys_.allocator};
	while (!is_kind(TokenKind::RBRACE) && !is_kind(TokenKind::ENDOF)) {
		if (is_kind(TokenKind::DIRECTIVE)) {
			eat(); // Eat <directive>
		} else if (is_keyword(KeywordKind::USING)) {
			eat(); // Eat 'using'
		}
		Array<AstRef<AstExpr>> lhs{sys_.allocator};
		for (;;) {
			auto ident = parse_ident_expr();
			if (!ident || !lhs.push_back(ident)) {
				return {};
			}
			if (is_kind(TokenKind::COMMA)) {
				eat(); // Eat ','
			} else {
				break;
			}
		}
		if (!is_operator(OperatorKind::COLON)) {
			error("Expected ':'");
			return {};
		}
		eat(); // Eat ':'
		auto type = parse_type_expr();
		if (!type) {
			return {};
		}
		auto decl = ast_.create<AstDeclStmt>(move(lhs), type, Maybe<Array<AstRef<AstExpr>>>{});
		if (!decl || !fields.push_back(decl)) {
			return {};
		}
		if (!is_kind(TokenKind::COMMA)) {
			break;
		}
		eat(); // Eat ','
	}
	if (is_kind(TokenKind::SEMICOLON)) {
		eat(); // Since ASI can add a semicolon for non-trailing comma
	}
	if (!is_kind(TokenKind::RBRACE)) {
		error("Expected '}'");
		return {};
	}
	eat(); // Eat '}'
	return ast_.create<AstStructExpr>(move(fields));
}

AstRef<AstExpr> Parser::parse_operand(Bool lhs) {
	TRACE();
	(void)lhs;
	if (is_kind(TokenKind::IDENTIFIER)) {
		return parse_ident_expr();
	} else if (is_kind(TokenKind::UNDEFINED)) {
		return parse_undef_expr();
	} else if (is_keyword(KeywordKind::CONTEXT)) {
		return parse_context_expr();
	} else if (is_keyword(KeywordKind::STRUCT)) {
		return parse_struct_expr();
	} else if (is_keyword(KeywordKind::PROC)) {
		return parse_proc_expr();
	}
	return {};
}

} // namespace Thor
