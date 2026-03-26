#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

class ReturnNode : public ASTNode
{
public:
	ASTNodePtr value;

	explicit ReturnNode(ASTNodePtr v = nullptr)
		: value(std::move(v))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

