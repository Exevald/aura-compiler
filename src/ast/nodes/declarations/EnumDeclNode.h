#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

#include <string>
#include <vector>

struct EnumVariantDecl
{
	std::string name;
	std::vector<std::string> argTypes;
};

class EnumDeclNode : public ASTNode
{
public:
	std::string name;
	std::vector<EnumVariantDecl> variants;

	EnumDeclNode(std::string enumName, std::vector<EnumVariantDecl> enumVariants)
		: name(std::move(enumName))
		, variants(std::move(enumVariants))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
