#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

#include <string>

class VarDeclNode : public ASTNode
{
public:
	std::string name;
	ASTNodePtr initializer;

	VarDeclNode(std::string n, ASTNodePtr i)
		: name(std::move(n))
		, initializer(std::move(i))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
