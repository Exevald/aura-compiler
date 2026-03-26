#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

#include <string>

class BinaryExprNode : public ASTNode
{
public:
	ASTNodePtr left;
	std::string op;
	ASTNodePtr right;

	BinaryExprNode(ASTNodePtr l, std::string o, ASTNodePtr r)
		: left(std::move(l))
		, op(std::move(o))
		, right(std::move(r))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

