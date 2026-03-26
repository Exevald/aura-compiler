#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

class WhileStatementNode : public ASTNode
{
public:
	ASTNodePtr condition;
	ASTNodePtr body;

	WhileStatementNode(ASTNodePtr c, ASTNodePtr b)
		: condition(std::move(c))
		, body(std::move(b))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

