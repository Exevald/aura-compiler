#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

#include <memory>
#include <string>
#include <vector>

struct Parameter
{
	std::string name;
	std::string typeName;
	std::shared_ptr<ASTNode> defaultValue;
	bool hasDefaultValue = false;
	bool isVariadic = false;

	Parameter() = default;
	Parameter(
		std::string paramName,
		std::string paramType,
		std::shared_ptr<ASTNode> initializer = nullptr,
		const bool variadic = false)
		: name(std::move(paramName))
		, typeName(std::move(paramType))
		, defaultValue(std::move(initializer))
		, hasDefaultValue(defaultValue != nullptr)
		, isVariadic(variadic)
	{
	}
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

enum class ContractKind
{
	Requires,
	Ensures,
	Invariant
};

class ContractNode : public ASTNode
{
public:
	ContractKind kind;
	ASTNodePtr expression;

	ContractNode(ContractKind contractKind, ASTNodePtr condition)
		: kind(contractKind)
		, expression(std::move(condition))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

class FunctionDeclNode : public ASTNode
{
public:
	std::string name;
	std::string returnType;
	bool isComptime = false;
	std::vector<TypeParameterDecl> typeParams;
	std::vector<Parameter> params;
	std::vector<ContextBinding> contextRequirements;
	std::vector<std::string> raisedEffects;
	std::vector<std::unique_ptr<ContractNode>> contracts;
	ASTNodePtr body;

	FunctionDeclNode(
		std::string n,
		std::string ret,
		bool comptime,
		std::vector<TypeParameterDecl> genericParams,
		std::vector<Parameter> p,
		std::vector<ContextBinding> contexts,
		std::vector<std::string> effects,
		std::vector<std::unique_ptr<ContractNode>> contractList,
		ASTNodePtr b)
		: name(std::move(n))
		, returnType(std::move(ret))
		, isComptime(comptime)
		, typeParams(std::move(genericParams))
		, params(std::move(p))
		, contextRequirements(std::move(contexts))
		, raisedEffects(std::move(effects))
		, contracts(std::move(contractList))
		, body(std::move(b))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
