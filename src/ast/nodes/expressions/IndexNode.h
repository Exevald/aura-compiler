#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

class IndexNode : public ASTNode
{
public:
	ASTNodePtr container;
	ASTNodePtr index;

	IndexNode(ASTNodePtr c, ASTNodePtr i)
		: container(std::move(c))
		, index(std::move(i))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

