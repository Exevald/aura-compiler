#pragma once

#include "../AST.h"

#include <string>

namespace EffectResumeAnalysis
{

inline size_t CountResumeCalls(const ASTNode* node)
{
	if (!node)
	{
		return 0;
	}

	if (const auto* call = dynamic_cast<const CallNode*>(node))
	{
		if (const auto* identifier = dynamic_cast<const IdentifierNode*>(call->callee.get());
			identifier && identifier->name == "resume")
		{
			return 1;
		}

		size_t count = CountResumeCalls(call->callee.get());
		for (const auto& arg : call->args)
		{
			count += CountResumeCalls(arg.get());
		}
		return count;
	}

	if (const auto* block = dynamic_cast<const BlockNode*>(node))
	{
		size_t count = 0;
		for (const auto& stmt : block->statements)
		{
			count += CountResumeCalls(stmt.get());
		}
		return count;
	}

	if (const auto* ifStmt = dynamic_cast<const IfStatementNode*>(node))
	{
		return CountResumeCalls(ifStmt->condition.get())
			+ CountResumeCalls(ifStmt->thenBlock.get())
			+ CountResumeCalls(ifStmt->elseBlock.get());
	}

	if (const auto* whileStmt = dynamic_cast<const WhileStatementNode*>(node))
	{
		return CountResumeCalls(whileStmt->condition.get())
			+ CountResumeCalls(whileStmt->body.get());
	}

	if (const auto* ret = dynamic_cast<const ReturnNode*>(node))
	{
		return CountResumeCalls(ret->value.get());
	}

	if (const auto* print = dynamic_cast<const PrintNode*>(node))
	{
		return CountResumeCalls(print->value.get());
	}

	if (const auto* unary = dynamic_cast<const UnaryExprNode*>(node))
	{
		return CountResumeCalls(unary->operand.get());
	}

	if (const auto* binary = dynamic_cast<const BinaryExprNode*>(node))
	{
		return CountResumeCalls(binary->left.get()) + CountResumeCalls(binary->right.get());
	}

	if (const auto* assignment = dynamic_cast<const AssignmentNode*>(node))
	{
		return CountResumeCalls(assignment->value.get())
			+ CountResumeCalls(assignment->index.get())
			+ CountResumeCalls(assignment->object.get())
			+ CountResumeCalls(assignment->dereferenceTarget.get());
	}

	if (const auto* member = dynamic_cast<const MemberAccessNode*>(node))
	{
		return CountResumeCalls(member->object.get());
	}

	if (const auto* index = dynamic_cast<const IndexNode*>(node))
	{
		return CountResumeCalls(index->container.get())
			+ CountResumeCalls(index->index.get());
	}

	if (const auto* array = dynamic_cast<const ArrayLiteralNode*>(node))
	{
		size_t count = 0;
		for (const auto& element : array->elements)
		{
			count += CountResumeCalls(element.get());
		}
		return count;
	}

	if (const auto* unsafeNode = dynamic_cast<const UnsafeNode*>(node))
	{
		return CountResumeCalls(unsafeNode->body.get());
	}

	return 0;
}

} // namespace EffectResumeAnalysis