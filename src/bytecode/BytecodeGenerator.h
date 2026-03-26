#pragma once

#include "../ast/AST.h"
#include "../ast/SymbolTable.h"
#include "../vm/core/OpCode.h"
#include "../vm/execution/Chunk.h"

class BytecodeGenerator : public ASTVisitor
{
public:
	VM::Execution::Chunk Compile(ASTNode* root);

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

	void Visit(ComptimeNode& node) override;
	void Visit(LeafNode& node) override;
	void Visit(RawNode& node) override;

private:
	void EmitBinaryOp(const std::string& op) const;
	[[nodiscard]] size_t EmitJump(VM::Core::OpCode opcode) const;
	void PatchJump(size_t jumpAddr) const;
	[[nodiscard]] VM::Execution::Chunk& CurrentChunk() const;

	SymbolTable m_symbols;
	std::shared_ptr<VM::Core::Function> m_currentFunction;
};