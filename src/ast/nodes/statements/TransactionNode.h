#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

#include <string>
#include <vector>

class TransactionNode : public ASTNode
{
public:
	struct Region
	{
		bool usesShared = false;
		std::string name;
	};

	std::vector<Region> regions;
	ASTNodePtr body;

	TransactionNode(std::vector<Region> transactionRegions, ASTNodePtr transactionBody)
		: regions(std::move(transactionRegions))
		, body(std::move(transactionBody))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
