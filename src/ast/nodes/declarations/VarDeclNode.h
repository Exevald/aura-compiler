#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

#include <string>

class VarDeclNode : public ASTNode
{
public:
	enum class StorageClass
	{
		Default,
		Shared,
		ThreadLocal
	};

	std::string name;
	std::string explicitType;
	ASTNodePtr initializer;
	bool isConst = false;
	StorageClass storageClass = StorageClass::Default;

	VarDeclNode(
		std::string n,
		std::string t,
		ASTNodePtr i,
		bool constant = false,
		StorageClass storage = StorageClass::Default)
		: name(std::move(n))
		, explicitType(std::move(t))
		, initializer(std::move(i))
		, isConst(constant)
		, storageClass(storage)
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
