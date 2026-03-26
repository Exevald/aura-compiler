#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

class PrintNode : public ASTNode
{
public:
	ASTNodePtr value;

	explicit PrintNode(ASTNodePtr v)
		: value(std::move(v))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

