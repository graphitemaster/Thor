#ifndef THOR_AST_H
#define THOR_AST_H
#include "util/slab.h"
#include "util/string.h"
#include "lexer.h"

namespace Thor {

struct Ast;
struct System;

struct AstSlabID {
	// Only 6-bit slab index (2^6 = 64)
	static inline constexpr const auto MAX = 64_u32;
	template<typename T>
	static Uint32 id() {
		static const Uint32 id = s_id++;
		return id;
	}
private:
	static inline Uint32 s_id;
};

struct AstNode {
	// Only 12-bit node index (2^12 = 4096)
	static inline constexpr const auto MAX = 4096_u32;
};

struct AstID {
	// Only 14-bit pool index (2^14 = 16384)
	static inline constexpr const auto MAX = 16384_u32;
	constexpr AstID() = default;
	constexpr AstID(Uint32 value)
		: value_{value}
	{
	}
	[[nodiscard]] constexpr Bool is_valid() const {
		return value_ != ~0_u32;
	}
	[[nodiscard]] constexpr operator Bool() const {
		return is_valid();
	}
private:
	template<typename>
	friend struct AstRef;
	friend struct Ast;
	Uint32 value_ = ~0_u32;
};
static_assert(sizeof(AstID) == 4);

template<typename T>
struct AstRef {
	constexpr AstRef() = default;
	constexpr AstRef(AstID id)
		: id_{id}
	{
	}
	[[nodiscard]] constexpr auto is_valid() const {
		return id_.is_valid();
	}
	[[nodiscard]] constexpr operator Bool() const {
		return is_valid();
	}
	template<typename U>
	[[nodiscard]] constexpr operator AstRef<U>() const 
		requires DerivedFrom<T, U>
	{
		return AstRef<U>(id_);
	}
private:
	friend struct Ast;
	AstID id_;
};

struct AstStmt : AstNode {
	enum struct Kind : Uint8 {
		EMPTY,
		BLOCK,
		IMPORT,
		PACKAGE,
		DEFER,
		BREAK,
		CONTINUE,
		FALLTHROUGH
	};

	constexpr AstStmt(Kind kind)
		: kind{kind}
	{
	}

	template<typename T>
	[[nodiscard]] constexpr Bool is_stmt() const {
		return kind == T::KIND;
	}

	virtual void dump(const Ast& ast, StringBuilder& builder, Ulen nest) const = 0;

	Kind kind;
};

struct AstEmptyStmt : AstStmt {
	static constexpr const auto KIND = Kind::EMPTY;
	constexpr AstEmptyStmt()
		: AstStmt{KIND}
	{
	}
	virtual void dump(const Ast& ast, StringBuilder& builder, Ulen nest) const;
};

struct AstBlockStmt : AstStmt {
	static constexpr const auto KIND = Kind::BLOCK;
	constexpr AstBlockStmt(Array<AstRef<AstStmt>>&& stmts)
		: AstStmt{KIND}
		, stmts{move(stmts)}
	{
	}
	virtual void dump(const Ast& ast, StringBuilder& builder, Ulen nest) const;
	Array<AstRef<AstStmt>> stmts;
};

struct AstImportStmt : AstStmt {
	static constexpr const auto KIND = Kind::IMPORT;
	constexpr AstImportStmt(StringView path)
		: AstStmt{KIND}
		, path{path}
	{
	}
	virtual void dump(const Ast& ast, StringBuilder& builder, Ulen nest) const;
	StringView path;
};

struct AstPackageStmt : AstStmt {
	static constexpr const auto KIND = Kind::PACKAGE;
	constexpr AstPackageStmt(StringView name)
		: AstStmt{KIND}
		, name{name}
	{
	}
	virtual void dump(const Ast& ast, StringBuilder& builder, Ulen nest) const;
	StringView name;
};

struct AstDeferStmt : AstStmt {
	static constexpr const auto KIND = Kind::DEFER;
	constexpr AstDeferStmt(AstRef<AstStmt> stmt)
		: AstStmt{KIND}
		, stmt{stmt}
	{
	}
	virtual void dump(const Ast& ast, StringBuilder& builder, Ulen nest) const;
	AstRef<AstStmt> stmt;
};

struct AstBreakStmt : AstStmt {
	static constexpr const auto KIND = Kind::BREAK;
	constexpr AstBreakStmt(StringView label)
		: AstStmt{KIND}
		, label{label}
	{
	}
	virtual void dump(const Ast& ast, StringBuilder& builder, Ulen nest) const;
	StringView label;
};

struct AstContinueStmt : AstStmt {
	static constexpr const auto KIND = Kind::CONTINUE;
	constexpr AstContinueStmt(StringView label)
		: AstStmt{KIND}
		, label{label}
	{
	}
	virtual void dump(const Ast& ast, StringBuilder& builder, Ulen nest) const;
	StringView label;
};

struct AstFallthroughStmt : AstStmt {
	static constexpr const auto KIND = Kind::FALLTHROUGH;
	constexpr AstFallthroughStmt()
		: AstStmt{KIND}
	{
	}
	virtual void dump(const Ast& ast, StringBuilder& builder, Ulen nest) const;
};

struct AstExpr : AstNode {
	virtual void dump(const Ast& ast, StringBuilder& builder) const = 0;
};

struct AstBinExpr : AstExpr {
	constexpr AstBinExpr(AstRef<AstExpr> lhs, AstRef<AstExpr> rhs, OperatorKind op)
		: lhs{lhs}
		, rhs{rhs}
		, op{op}
	{
	}
	virtual void dump(const Ast& ast, StringBuilder& builder) const;
	AstRef<AstExpr> lhs;
	AstRef<AstExpr> rhs;
	OperatorKind    op;
};

struct AstUnaryExpr : AstExpr {
	constexpr AstUnaryExpr(AstRef<AstExpr> operand, OperatorKind op)
		: operand{operand}
		, op{op}
	{
	}
	virtual void dump(const Ast& ast, StringBuilder& builder) const;
	AstRef<AstExpr> operand;
	OperatorKind    op;
};

struct AstTernaryExpr : AstExpr {
	constexpr AstTernaryExpr(AstRef<AstExpr> cond, AstRef<AstExpr> on_true, AstRef<AstExpr> on_false)
		: cond{cond}
		, on_true{on_true}
		, on_false{on_false}
	{
	}
	virtual void dump(const Ast& ast, StringBuilder& builder) const;
	AstRef<AstExpr> cond;
	AstRef<AstExpr> on_true;
	AstRef<AstExpr> on_false;
};

struct AstIdentExpr : AstExpr {
	constexpr AstIdentExpr(StringView ident)
		: ident{ident}
	{
	}
	virtual void dump(const Ast& ast, StringBuilder& builder) const;
	StringView ident;
};

struct AstUndefExpr : AstExpr {
	virtual void dump(const Ast& ast, StringBuilder& builder) const;
};

struct AstContextExpr : AstExpr {
	virtual void dump(const Ast& ast, StringBuilder& builder) const;
};

struct AstType : AstNode {
	constexpr AstType(AstRef<AstExpr> expr)
		: expr{expr}
	{
	}
	AstRef<AstExpr> expr;
};

struct Ast {
	constexpr Ast(Allocator& allocator)
		: slabs_{allocator}
	{
	}

	static inline constexpr const auto MAX = AstID::MAX * AstNode::MAX;

	template<typename T, typename... Ts>
	AstRef<T> create(Ts&&... args) {
		auto slab_idx = AstSlabID::id<T>();
		if (slab_idx >= slabs_.length() && !slabs_.resize(slab_idx + 1)) {
			return {};
		}
		auto& slab = slabs_[slab_idx];
		if (!slab) {
			slab.emplace(slabs_.allocator(), sizeof(T), AstNode::MAX);
		}
		if (auto slab_ref = slab->allocate()) {
			new ((*slab)[*slab_ref], Nat{}) T{forward<Ts>(args)...};
			return AstID { slab_idx * MAX + slab_ref->index };
		}
		return {};
	}

	template<typename T>
	constexpr const T& operator[](AstRef<T> ref) const {
		const auto slab_idx = ref.id_.value_ / MAX;
		const auto slab_ref = ref.id_.value_ % MAX;
		return *reinterpret_cast<const T*>((*slabs_[slab_idx])[SlabRef { slab_ref }]);
	}

private:
	Array<Maybe<Slab>> slabs_;
};


} // namespace Thor

#endif // THOR_AST_H