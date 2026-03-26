#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

#include <vector>

class BlockNode : public ASTNode
{
public:
	std::vector<ASTNodePtr> statements;
	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};

