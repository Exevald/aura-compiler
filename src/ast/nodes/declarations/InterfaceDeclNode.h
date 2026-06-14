#pragma once

#include "FunctionDeclNode.h"
#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

#include <string>
#include <vector>

struct InterfaceMethodSig
{
	std::string name;
	std::string returnType;
	std::vector<Parameter> params;
	std::vector<std::string> raisedEffects;
};

class InterfaceDeclNode : public ASTNode
{
public:
	std::string name;
	std::vector<TypeParameterDecl> typeParams;
	std::vector<InterfaceMethodSig> methods;

	InterfaceDeclNode(
		std::string interfaceName,
		std::vector<TypeParameterDecl> genericParams,
		std::vector<InterfaceMethodSig> methodDecls)
		: name(std::move(interfaceName))
		, typeParams(std::move(genericParams))
		, methods(std::move(methodDecls))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
