#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"
#include "FunctionDeclNode.h"

#include <string>
#include <vector>

class TypeAliasNode : public ASTNode
{
public:
	std::string name;
	std::string aliasedType;
	std::vector<TypeParameterDecl> typeParams;

	TypeAliasNode(
		std::string aliasName,
		std::string targetType,
		std::vector<TypeParameterDecl> genericParams = {})
		: name(std::move(aliasName))
		, aliasedType(std::move(targetType))
		, typeParams(std::move(genericParams))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
