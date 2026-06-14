#pragma once

#include "FunctionDeclNode.h"
#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

#include <string>
#include <vector>

struct ActorFieldDecl
{
	std::string name;
	std::string typeName;
	ASTNodePtr initializer;
};

struct ActorMethodDecl
{
	enum class Kind
	{
		Message,
		Query
	};

	Kind kind = Kind::Message;
	std::string name;
	std::string returnType;
	std::vector<Parameter> params;
	std::vector<ContextBinding> contextRequirements;
	std::vector<std::string> raisedEffects;
	std::vector<std::unique_ptr<ContractNode>> contracts;
	ASTNodePtr body;

	ActorMethodDecl(
		const Kind methodKind,
		std::string methodName,
		std::string retType,
		std::vector<Parameter> methodParams,
		std::vector<ContextBinding> contexts,
		std::vector<std::string> effects,
		std::vector<std::unique_ptr<ContractNode>> contractList,
		ASTNodePtr methodBody)
		: kind(methodKind)
		, name(std::move(methodName))
		, returnType(std::move(retType))
		, params(std::move(methodParams))
		, contextRequirements(std::move(contexts))
		, raisedEffects(std::move(effects))
		, contracts(std::move(contractList))
		, body(std::move(methodBody))
	{
	}

	ActorMethodDecl(ActorMethodDecl&&) noexcept = default;
	ActorMethodDecl& operator=(ActorMethodDecl&&) noexcept = default;
	ActorMethodDecl(const ActorMethodDecl&) = delete;
	ActorMethodDecl& operator=(const ActorMethodDecl&) = delete;
};

class ActorDeclNode : public ASTNode
{
public:
	std::string name;
	std::vector<TypeParameterDecl> typeParams;
	std::vector<ActorFieldDecl> fields;
	std::vector<ActorMethodDecl> methods;

	ActorDeclNode(
		std::string actorName,
		std::vector<TypeParameterDecl> genericParams,
		std::vector<ActorFieldDecl> actorFields,
		std::vector<ActorMethodDecl> actorMethods)
		: name(std::move(actorName))
		, typeParams(std::move(genericParams))
		, fields(std::move(actorFields))
		, methods(std::move(actorMethods))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
