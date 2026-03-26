#pragma once

#include <memory>

class ASTVisitor;

class ASTNode
{
public:
	virtual ~ASTNode() = default;
	virtual void Accept(ASTVisitor& visitor) = 0;
};

using ASTNodePtr = std::unique_ptr<ASTNode>;
