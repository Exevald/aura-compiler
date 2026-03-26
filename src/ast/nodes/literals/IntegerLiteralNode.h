#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

#include <cstdint>

class IntegerLiteralNode : public ASTNode
{
public:
	int64_t value;
	explicit IntegerLiteralNode(int64_t v)
		: value(v)
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

