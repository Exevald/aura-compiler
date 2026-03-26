#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

#include <string>
#include <vector>

class RawNode : public ASTNode
{
public:
	std::string ruleName;
	std::vector<ASTNodePtr> children;

	explicit RawNode(std::string name)
		: ruleName(std::move(name))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
