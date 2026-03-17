#pragma once

#include "../ast/AST.h"
#include "../ast/ASTVisitor.h"
#include "../ast/SymbolTable.h"
#include "../vm/core/OpCode.h"
#include "../vm/execution/Chunk.h"

class BytecodeGenerator : public ASTVisitor
{
public:
	VM::Execution::Chunk Compile(ASTNode* root);

	void Visit(LeafNode& node) override;
	void Visit(InternalNode& node) override;

private:
	void EmitBinaryOp(const std::string& op) const;
	void HandleIf(const InternalNode& node);
	void HandleWhile(InternalNode& node);

	[[nodiscard]] VM::Execution::Chunk& CurrentChunk() const;

	void HandleFunctionDecl(InternalNode& node);
	void HandleReturn(const InternalNode& node);
	uint8_t CountAndEmitArgs(ASTNode* node);
	void HandleCall(const InternalNode& node) const;

	SymbolTable m_symbols;
	std::shared_ptr<VM::Core::Function> m_currentFunction;
	std::vector<std::shared_ptr<VM::Core::Function>> m_functionStack;
};