#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

#include <string>

class IterNode : public ASTNode
{
public:
	std::string varName;
	ASTNodePtr collection;
	ASTNodePtr body;

	IterNode(std::string n, ASTNodePtr coll, ASTNodePtr b)
		: varName(std::move(n))
		, collection(std::move(coll))
		, body(std::move(b))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

