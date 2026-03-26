#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

#include <string>
#include <vector>

class CallNode : public ASTNode
{
public:
	std::string callee;
	std::vector<ASTNodePtr> args;

	CallNode(std::string c, std::vector<ASTNodePtr> a)
		: callee(std::move(c))
		, args(std::move(a))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

