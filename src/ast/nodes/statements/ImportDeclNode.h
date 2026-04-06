#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

#include <string>

class ImportDeclNode : public ASTNode
{
public:
	std::string qualifiedName;
	std::string alias;

	ImportDeclNode(std::string name, std::string importedAlias)
		: qualifiedName(std::move(name))
		, alias(std::move(importedAlias))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
