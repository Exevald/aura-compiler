#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

#include <string>

class ModuleDeclNode : public ASTNode
{
public:
	std::string qualifiedName;

	explicit ModuleDeclNode(std::string name)
		: qualifiedName(std::move(name))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
