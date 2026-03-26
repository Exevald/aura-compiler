#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

#include <string>

class AssignmentNode : public ASTNode
{
public:
	std::string name;
	ASTNodePtr value;
	ASTNodePtr index;

	AssignmentNode(std::string n, ASTNodePtr v, ASTNodePtr idx = nullptr)
		: name(std::move(n))
		, value(std::move(v))
		, index(std::move(idx))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

