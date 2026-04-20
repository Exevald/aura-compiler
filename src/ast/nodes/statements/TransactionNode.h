#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

#include <string>

class TransactionNode : public ASTNode
{
public:
	bool usesSharedRegion = false;
	std::string regionName;
	ASTNodePtr body;

	TransactionNode(bool sharedRegion, std::string name, ASTNodePtr transactionBody)
		: usesSharedRegion(sharedRegion)
		, regionName(std::move(name))
		, body(std::move(transactionBody))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
