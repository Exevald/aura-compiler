#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

#include <string>
#include <utility>
#include <vector>

class MapLiteralNode : public ASTNode
{
public:
	std::string keyTypeName;
	std::string valueTypeName;
	std::vector<std::pair<ASTNodePtr, ASTNodePtr>> entries;

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
