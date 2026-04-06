#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

class UnsafeNode : public ASTNode
{
public:
	ASTNodePtr body;

	explicit UnsafeNode(ASTNodePtr unsafeBody)
		: body(std::move(unsafeBody))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
