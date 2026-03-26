#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

#include <string>

class StringLiteralNode : public ASTNode
{
public:
	std::string value;
	explicit StringLiteralNode(std::string v)
		: value(std::move(v))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

