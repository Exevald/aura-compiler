#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

class ComptimeNode : public ASTNode
{
public:
	ASTNodePtr body;

	explicit ComptimeNode(ASTNodePtr b)
		: body(std::move(b))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
