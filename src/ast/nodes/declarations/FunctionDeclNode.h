#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

#include <string>
#include <vector>

struct Parameter
{
	std::string name;
	std::string typeName;
};

struct TypeParameterDecl
{
	std::string name;
	std::vector<std::string> constraints;
};

class FunctionDeclNode : public ASTNode
{
public:
	std::string name;
	std::string returnType;
	std::vector<TypeParameterDecl> typeParams;
	std::vector<Parameter> params;
	ASTNodePtr body;
	std::vector<ASTNodePtr> metadata;

	FunctionDeclNode(
		std::string n,
		std::string ret,
		std::vector<TypeParameterDecl> genericParams,
		std::vector<Parameter> p,
		ASTNodePtr b)
		: name(std::move(n))
		, returnType(std::move(ret))
		, typeParams(std::move(genericParams))
		, params(std::move(p))
		, body(std::move(b))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
