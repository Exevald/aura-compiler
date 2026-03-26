#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

class IfStatementNode : public ASTNode
{
public:
	ASTNodePtr condition;
	ASTNodePtr thenBlock;
	ASTNodePtr elseBlock;

	IfStatementNode(ASTNodePtr c, ASTNodePtr t, ASTNodePtr e = nullptr)
		: condition(std::move(c))
		, thenBlock(std::move(t))
		, elseBlock(std::move(e))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

