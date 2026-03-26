#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

#include <vector>

class ArrayLiteralNode : public ASTNode
{
public:
	std::vector<ASTNodePtr> elements;

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

