#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

#include <vector>

class CallNode : public ASTNode
{
public:
	ASTNodePtr callee;
	std::vector<ASTNodePtr> args;

	CallNode(ASTNodePtr c, std::vector<ASTNodePtr> a)
		: callee(std::move(c))
		, args(std::move(a))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
