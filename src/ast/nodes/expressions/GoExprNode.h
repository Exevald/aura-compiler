#pragma once

#include "../../core/ASTNode.h"
#include "../../core/ASTVisitor.h"

class GoExprNode : public ASTNode
{
public:
	ASTNodePtr call;

	explicit GoExprNode(ASTNodePtr expr)
		: call(std::move(expr))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
