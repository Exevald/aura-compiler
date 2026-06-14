#pragma once

#include "../declarations/FunctionDeclNode.h"

class FunctionExprNode : public ASTNode
{
public:
	std::string name;
	std::string returnType;
	std::vector<Parameter> params;
	std::vector<std::string> raisedEffects;
	ASTNodePtr body;

	FunctionExprNode(
		std::string n,
		std::string ret,
		std::vector<Parameter> p,
		std::vector<std::string> effects,
		ASTNodePtr b)
		: name(std::move(n))
		, returnType(std::move(ret))
		, params(std::move(p))
		, raisedEffects(std::move(effects))
		, body(std::move(b))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
