#ifndef THOR_PARSER_H
#define THOR_PARSER_H
#include "lexer.h"
#include "ast.h"
#include "util/system.h"

namespace Thor {

struct Parser {
	static Maybe<Parser> open(System& sys, StringView file);
	StringView parse_ident();

	// Expression parsers
	AstRef<AstIdentExpr>   parse_ident_expr();
	AstRef<AstUndefExpr>   parse_undef_expr();
	AstRef<AstContextExpr> parse_context_expr();

	AstRef<AstExpr>       parse_expr(Bool lhs);
	AstRef<AstExpr>       parse_operand();
	AstRef<AstExpr>       parse_bin_expr(Bool lhs, Uint32 prec);
	AstRef<AstExpr>       parse_unary_expr(Bool lhs);
	AstRef<AstExpr>       parse_operand(Bool lhs); // Operand parser for AstBinExpr or AstUnaryExpr
	AstRef<AstStructExpr> parse_struct_expr();
	AstRef<AstTypeExpr>   parse_type_expr();

	// Statement parsers
	AstRef<AstStmt>            parse_stmt();
	AstRef<AstStmt>            parse_simple_stmt();
	AstRef<AstEmptyStmt>       parse_empty_stmt();
	AstRef<AstBlockStmt>       parse_block_stmt();
	AstRef<AstPackageStmt>     parse_package_stmt();
	AstRef<AstImportStmt>      parse_import_stmt();
	AstRef<AstBreakStmt>       parse_break_stmt();
	AstRef<AstContinueStmt>    parse_continue_stmt();
	AstRef<AstFallthroughStmt> parse_fallthrough_stmt();
	AstRef<AstIfStmt>          parse_if_stmt();
	AstRef<AstDeferStmt>       parse_defer_stmt();
	AstRef<AstProc>            parse_proc();

	[[nodiscard]] constexpr AstFile& ast() { return ast_; }
	[[nodiscard]] constexpr const AstFile& ast() const { return ast_; }
private:
	Maybe<Array<AstRef<AstExpr>>> parse_expr_list(Bool lhs);
	AstRef<AstExpr> parse_unary_atom(AstRef<AstExpr> operand, Bool lhs);

	Bool skip_possible_newline_for_literal();

	Parser(System& sys, Lexer&& lexer);

	template<Ulen E, typename... Ts>
	void error(const char (&msg)[E], Ts&&...) {
		sys_.console.write(sys_, StringView { msg });
		sys_.console.write(sys_, StringView { "\n" });
		sys_.console.flush(sys_);
	}
	constexpr Bool is_kind(TokenKind kind) const {
		return token_.kind == kind;
	}
	constexpr Bool is_keyword(KeywordKind kind) const {
		return is_kind(TokenKind::KEYWORD) && token_.as_keyword == kind;
	}
	constexpr Bool is_operator(OperatorKind kind) const {
		return is_kind(TokenKind::OPERATOR) && token_.as_operator == kind;
	}
	constexpr Bool is_literal() const {
		return is_kind(TokenKind::LITERAL);
	}
	constexpr Bool is_literal(LiteralKind kind) const {
		return is_kind(TokenKind::LITERAL) && token_.as_literal == kind;
	}
	constexpr Bool is_assignment() const {
		return is_kind(TokenKind::ASSIGNMENT);
	}
	constexpr Bool is_assignment(AssignKind kind) const {
		return is_kind(TokenKind::ASSIGNMENT) && token_.as_assign == kind;
	}
	void eat() {
		token_ = lexer_.next();
	}
	System&            sys_;
	TemporaryAllocator temporary_;
	AstFile            ast_;
	Lexer              lexer_;
	Token              token_;
	// >= 0: In Expression
	// <  0: In Control Clause
	Sint32             expr_level_ = 0;
	Bool               allow_in_expr_ = false;
};

} // namespace Thor

#endif // THOR_PARSER_H