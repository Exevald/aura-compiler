#pragma once

#include "../../core/ASTNode.h"
#include "../../core/ASTVisitor.h"

class AwaitExprNode : public ASTNode
{
public:
	ASTNodePtr operand;

	explicit AwaitExprNode(ASTNodePtr value)
		: operand(std::move(value))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
