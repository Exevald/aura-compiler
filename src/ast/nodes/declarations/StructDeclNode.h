#pragma once

#include "FunctionDeclNode.h"
#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

#include <string>
#include <vector>

struct StructField
{
	std::string name;
	std::string typeName;
};

struct StructMethodDecl
{
	std::string name;
	std::string returnType;
	std::vector<Parameter> params;
	std::vector<ContextBinding> contextRequirements;
	std::vector<std::string> raisedEffects;
	std::vector<std::unique_ptr<ContractNode>> contracts;
	ASTNodePtr body;

	StructMethodDecl(
		std::string methodName,
		std::string retType,
		std::vector<Parameter> methodParams,
		std::vector<ContextBinding> contexts,
		std::vector<std::string> effects,
		std::vector<std::unique_ptr<ContractNode>> contractList,
		ASTNodePtr methodBody)
		: name(std::move(methodName))
		, returnType(std::move(retType))
		, params(std::move(methodParams))
		, contextRequirements(std::move(contexts))
		, raisedEffects(std::move(effects))
		, contracts(std::move(contractList))
		, body(std::move(methodBody))
	{
	}

	StructMethodDecl(StructMethodDecl&&) noexcept = default;
	StructMethodDecl& operator=(StructMethodDecl&&) noexcept = default;
	StructMethodDecl(const StructMethodDecl&) = delete;
	StructMethodDecl& operator=(const StructMethodDecl&) = delete;
};

class StructDeclNode : public ASTNode
{
public:
	std::string name;
	std::vector<TypeParameterDecl> typeParams;
	std::vector<StructField> fields;
	std::vector<std::string> implementedInterfaces;
	std::vector<StructMethodDecl> methods;
	std::vector<std::unique_ptr<ContractNode>> contracts;

	StructDeclNode(
		std::string structName,
		std::vector<TypeParameterDecl> genericParams,
		std::vector<StructField> structFields,
		std::vector<std::string> interfaces = {},
		std::vector<StructMethodDecl> structMethods = {},
		std::vector<std::unique_ptr<ContractNode>> contractList = {})
		: name(std::move(structName))
		, typeParams(std::move(genericParams))
		, fields(std::move(structFields))
		, implementedInterfaces(std::move(interfaces))
		, methods(std::move(structMethods))
		, contracts(std::move(contractList))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
