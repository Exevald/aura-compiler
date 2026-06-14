#pragma once

#include "../declarations/FunctionDeclNode.h"
#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

#include <string>
#include <vector>

struct EffectHandlerCase
{
	std::string effectName;
	std::string effectKey;
	std::vector<Parameter> params;
	ASTNodePtr body;

	EffectHandlerCase(std::string name, std::vector<Parameter> handlerParams, ASTNodePtr handlerBody)
		: effectName(std::move(name))
		, effectKey()
		, params(std::move(handlerParams))
		, body(std::move(handlerBody))
	{
	}

	EffectHandlerCase(EffectHandlerCase&&) noexcept = default;
	EffectHandlerCase& operator=(EffectHandlerCase&&) noexcept = default;
	EffectHandlerCase(const EffectHandlerCase&) = delete;
	EffectHandlerCase& operator=(const EffectHandlerCase&) = delete;
};

class HandleNode : public ASTNode
{
public:
	ASTNodePtr expression;
	std::vector<EffectHandlerCase> handlers;

	HandleNode(ASTNodePtr handledExpression, std::vector<EffectHandlerCase> effectHandlers)
		: expression(std::move(handledExpression))
		, handlers(std::move(effectHandlers))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
