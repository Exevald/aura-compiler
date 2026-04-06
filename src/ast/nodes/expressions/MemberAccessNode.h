#pragma once

#include "../../core/ASTNode.h"
#include "../../core/ASTVisitor.h"

#include <string>

class MemberAccessNode : public ASTNode
{
public:
	ASTNodePtr object;
	std::string member;

	MemberAccessNode(ASTNodePtr obj, std::string name)
		: object(std::move(obj))
		, member(std::move(name))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
