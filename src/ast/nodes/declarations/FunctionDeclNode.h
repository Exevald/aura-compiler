#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

#include <string>
#include <vector>

struct Parameter
{
	std::string name;
};

class FunctionDeclNode : public ASTNode
{
public:
	std::string name;
	std::vector<Parameter> params;
	ASTNodePtr body;
	std::vector<ASTNodePtr> metadata;

	FunctionDeclNode(std::string n, std::vector<Parameter> p, ASTNodePtr b)
		: name(std::move(n))
		, params(std::move(p))
		, body(std::move(b))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
