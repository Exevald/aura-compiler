#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

#include <string>

class AssignmentNode : public ASTNode
{
public:
	std::string name;
	ASTNodePtr object;
	std::string member;
	ASTNodePtr dereferenceTarget;
	ASTNodePtr value;
	ASTNodePtr index;

	AssignmentNode(std::string n, ASTNodePtr v, ASTNodePtr idx = nullptr)
		: name(std::move(n))
		, value(std::move(v))
		, index(std::move(idx))
	{
	}

	AssignmentNode(ASTNodePtr obj, std::string memberName, ASTNodePtr v)
		: object(std::move(obj))
		, member(std::move(memberName))
		, value(std::move(v))
	{
	}

	AssignmentNode(ASTNodePtr derefTarget, ASTNodePtr v)
		: dereferenceTarget(std::move(derefTarget))
		, value(std::move(v))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
