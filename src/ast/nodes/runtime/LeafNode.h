#pragma once

#include "core/ASTNode.h"

#include <string>

class LeafNode : public ASTNode
{
public:
	std::string type;
	std::string value;

	LeafNode(std::string t, std::string v)
		: type(std::move(t))
		, value(std::move(v))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
