#pragma once

#include "../../core/ASTNode.h"
#include "../../core/ASTVisitor.h"

#include <string>

class UnaryExprNode : public ASTNode
{
public:
	std::string op;
	ASTNodePtr operand;

	UnaryExprNode(std::string unaryOp, ASTNodePtr expr)
		: op(std::move(unaryOp))
		, operand(std::move(expr))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
