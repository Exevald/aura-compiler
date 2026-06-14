#pragma once

#include "AST.h"
#include "OpCode.h"

#include <filesystem>
#include <string>

using namespace VM::Core;

class ASTSearcher : public ASTVisitor
{
public:
	std::string targetIdentifier;
	std::string targetValue;
	std::string targetRule;
	bool foundType = false;
	bool foundIdentifier = false;
	bool foundValue = false;
	bool foundRule = false;
	const std::type_info* targetType = nullptr;

	void Check(ASTNode& node);

	void Visit(IntegerLiteralNode& n) override;
	void Visit(FloatLiteralNode& n) override;
	void Visit(StringLiteralNode& n) override;
	void Visit(IdentifierNode& n) override;
	void Visit(UnaryExprNode& n) override;
	void Visit(BinaryExprNode& n) override;
	void Visit(AssignmentNode& n) override;
	void Visit(VarDeclNode& n) override;
	void Visit(TypeAliasNode& n) override;
	void Visit(InterfaceDeclNode& n) override;
	void Visit(EffectDeclNode& n) override;
	void Visit(ActorDeclNode& n) override;
	void Visit(StructDeclNode& n) override;
	void Visit(EnumDeclNode& n) override;
	void Visit(BlockNode& n) override;
	void Visit(ExportDeclNode& n) override;
	void Visit(IfStatementNode& n) override;
	void Visit(WhileStatementNode& n) override;
	void Visit(FunctionDeclNode& n) override;
	void Visit(FunctionExprNode& n) override;
	void Visit(CallNode& n) override;
	void Visit(GoExprNode& n) override;
	void Visit(AwaitExprNode& n) override;
	void Visit(MemberAccessNode& n) override;
	void Visit(ModuleDeclNode& n) override;
	void Visit(ImportDeclNode& n) override;
	void Visit(ReturnNode& n) override;
	void Visit(PrintNode& n) override;
	void Visit(UnsafeNode& n) override;
	void Visit(ArrayLiteralNode& n) override;
	void Visit(MapLiteralNode& n) override;
	void Visit(IndexNode& n) override;
	void Visit(IterNode& n) override;
	void Visit(TransactionNode& n) override;
	void Visit(HandleNode& n) override;
	void Visit(RawNode& n) override;
	void Visit(LeafNode& n) override;
	void Visit(ComptimeNode& n) override;
	void Visit(ContractNode& n) override;
};
