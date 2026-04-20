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

struct ContextBinding
{
	std::string name;
	std::string typeName;
};

class FunctionDeclNode : public ASTNode
{
public:
	std::string name;
	std::string returnType;
	std::vector<TypeParameterDecl> typeParams;
	std::vector<Parameter> params;
	std::vector<ContextBinding> contextRequirements;
	std::vector<std::string> raisedEffects;
	ASTNodePtr body;
	std::vector<ASTNodePtr> metadata;

	FunctionDeclNode(
		std::string n,
		std::string ret,
		std::vector<TypeParameterDecl> genericParams,
		std::vector<Parameter> p,
		std::vector<ContextBinding> contexts,
		std::vector<std::string> effects,
		ASTNodePtr b)
		: name(std::move(n))
		, returnType(std::move(ret))
		, typeParams(std::move(genericParams))
		, params(std::move(p))
		, contextRequirements(std::move(contexts))
		, raisedEffects(std::move(effects))
		, body(std::move(b))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
