#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

#include <string>

class ExportDeclNode : public ASTNode
{
public:
	ASTNodePtr declaration;
	std::string exportedName;

	explicit ExportDeclNode(ASTNodePtr exportedDecl)
		: declaration(std::move(exportedDecl))
	{
	}

	explicit ExportDeclNode(std::string name)
		: exportedName(std::move(name))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
