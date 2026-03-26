#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

#include <string>

class IdentifierNode : public ASTNode
{
public:
	std::string name;
	explicit IdentifierNode(std::string n)
		: name(std::move(n))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

