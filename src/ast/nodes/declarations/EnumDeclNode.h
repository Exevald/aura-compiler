#pragma once

#include "FunctionDeclNode.h"
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
	std::vector<TypeParameterDecl> typeParams;
	std::vector<EnumVariantDecl> variants;

	EnumDeclNode(
		std::string enumName,
		std::vector<TypeParameterDecl> genericParams,
		std::vector<EnumVariantDecl> enumVariants)
		: name(std::move(enumName))
		, typeParams(std::move(genericParams))
		, variants(std::move(enumVariants))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
