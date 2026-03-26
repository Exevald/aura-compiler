#pragma once

#include "AST.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class SemanticAnalyzer : public ASTVisitor
{
public:
	void Analyze(ASTNode* root);

private:
	enum class TypeKind
	{
		Unknown,
		Void,
		Bool,
		Int,
		Float,
		String,
		Array,
		Function
	};

	struct TypeInfo
	{
		TypeKind kind{ TypeKind::Unknown };
		std::shared_ptr<TypeInfo> element;

		static TypeInfo Unknown() { return {}; }
		static TypeInfo Void() { return { TypeKind::Void }; }
		static TypeInfo Bool() { return { TypeKind::Bool }; }
		static TypeInfo Int() { return { TypeKind::Int }; }
		static TypeInfo Float() { return { TypeKind::Float }; }
		static TypeInfo String() { return { TypeKind::String }; }
		static TypeInfo Function() { return { TypeKind::Function }; }
		static TypeInfo ArrayOf(TypeInfo elem)
		{
			TypeInfo t;
			t.kind = TypeKind::Array;
			t.element = std::make_shared<TypeInfo>(std::move(elem));
			return t;
		}
	};

	using Env = std::unordered_map<std::string, TypeInfo>;

	TypeInfo AnalyzeExpr(ASTNode* node);

	static bool IsPrimitiveNumeric(const TypeInfo& t);
	static bool IsTruthyBinaryOp(const std::string& op);
	void EnsureDeclared(const std::string& name);
	[[nodiscard]] TypeInfo Resolve(const std::string& name) const;
	void Define(const std::string& name, TypeInfo type);

	void AnalyzeTypeForBinaryOp(const std::string& op, const TypeInfo& lhs, const TypeInfo& rhs);
	void AnalyzeAssignment(ASTNode* valueExpr, const std::string& name, ASTNode* indexExpr, bool hasIndex);

	TypeInfo m_lastType;

	std::vector<Env> m_envStack;

	TypeInfo VisitAndGet(ASTNode* node);

	void Visit(IntegerLiteralNode& node) override;
	void Visit(FloatLiteralNode& node) override;
	void Visit(StringLiteralNode& node) override;
	void Visit(IdentifierNode& node) override;
	void Visit(BinaryExprNode& node) override;
	void Visit(AssignmentNode& node) override;
	void Visit(VarDeclNode& node) override;
	void Visit(BlockNode& node) override;
	void Visit(IfStatementNode& node) override;
	void Visit(WhileStatementNode& node) override;
	void Visit(FunctionDeclNode& node) override;
	void Visit(CallNode& node) override;
	void Visit(ReturnNode& node) override;
	void Visit(PrintNode& node) override;
	void Visit(ArrayLiteralNode& node) override;
	void Visit(IndexNode& node) override;
	void Visit(IterNode& node) override;
	void Visit(LeafNode& node) override;
	void Visit(RawNode& node) override;
	void Visit(ComptimeNode& node) override;
};
