#pragma once

#include "../declarations/FunctionDeclNode.h"

class FunctionExprNode : public ASTNode
{
public:
	std::string name;
	std::string returnType;
	std::vector<Parameter> params;
	ASTNodePtr body;

	FunctionExprNode(std::string n, std::string ret, std::vector<Parameter> p, ASTNodePtr b)
		: name(std::move(n))
		, returnType(std::move(ret))
		, params(std::move(p))
		, body(std::move(b))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
