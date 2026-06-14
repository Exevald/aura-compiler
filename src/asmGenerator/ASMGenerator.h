#pragma once

#include "../ast/AST.h"
#include "core/ASTVisitor.h"

#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class ASMGenerator : public ASTVisitor
{
public:
	explicit ASMGenerator(std::ofstream outFile);
	void Compile(ASTNode* root);

	void Visit(IntegerLiteralNode& node) override;
	void Visit(FloatLiteralNode& node) override;
	void Visit(IdentifierNode& node) override;
	void Visit(AssignmentNode& node) override;
	void Visit(VarDeclNode& node) override;
	void Visit(IfStatementNode& node) override;
	void Visit(StringLiteralNode& node) override;
	void Visit(UnaryExprNode& node) override;
	void Visit(BinaryExprNode& node) override;
	void Visit(TypeAliasNode& node) override;
	void Visit(StructDeclNode& node) override;
	void Visit(EnumDeclNode& node) override;
	void Visit(InterfaceDeclNode& node) override;
	void Visit(EffectDeclNode& node) override;
	void Visit(ActorDeclNode& node) override;
	void Visit(BlockNode& node) override;
	void Visit(ExportDeclNode& node) override;
	void Visit(WhileStatementNode& node) override;
	void Visit(FunctionDeclNode& node) override;
	void Visit(FunctionExprNode& node) override;
	void Visit(CallNode& node) override;
	void Visit(GoExprNode& node) override;
	void Visit(AwaitExprNode& node) override;
	void Visit(MemberAccessNode& node) override;
	void Visit(ModuleDeclNode& node) override;
	void Visit(ImportDeclNode& node) override;
	void Visit(ReturnNode& node) override;
	void Visit(PrintNode& node) override;
	void Visit(UnsafeNode& node) override;
	void Visit(ArrayLiteralNode& node) override;
	void Visit(MapLiteralNode& node) override;
	void Visit(IndexNode& node) override;
	void Visit(IterNode& node) override;
	void Visit(TransactionNode& node) override;
	void Visit(HandleNode& node) override;
	void Visit(LeafNode& node) override;
	void Visit(RawNode& node) override;
	void Visit(ComptimeNode& node) override;
	void Visit(ContractNode& node) override;

private:
	void EmitInstruction(const std::string& instruction);
	[[nodiscard]] std::string NextLabel();

	std::ofstream m_outFile;
	size_t m_labelCounter = 0;
};
