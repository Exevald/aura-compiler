#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

class FloatLiteralNode : public ASTNode
{
public:
	double value;
	explicit FloatLiteralNode(double v)
		: value(v)
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

