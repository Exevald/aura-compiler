#pragma once

#include "InterfaceDeclNode.h"
#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

#include <string>
#include <vector>

class EffectDeclNode : public ASTNode
{
public:
	std::string name;
	std::vector<InterfaceMethodSig> operations;

	EffectDeclNode(std::string effectName, std::vector<InterfaceMethodSig> effectOperations)
		: name(std::move(effectName))
		, operations(std::move(effectOperations))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
