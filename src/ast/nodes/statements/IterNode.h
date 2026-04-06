#pragma once

#include "core/ASTNode.h"
#include "core/ASTVisitor.h"

#include <string>
#include <vector>

enum class IterAdapterKind
{
	Drop,
	Take,
	Reverse,
	Filter,
	Transform
};

struct IterAdapter
{
	IterAdapterKind kind;
	ASTNodePtr argument;

	IterAdapter(IterAdapterKind adapterKind, ASTNodePtr adapterArgument = nullptr)
		: kind(adapterKind)
		, argument(std::move(adapterArgument))
	{
	}

	IterAdapter(IterAdapter&&) noexcept = default;
	IterAdapter& operator=(IterAdapter&&) noexcept = default;

	IterAdapter(const IterAdapter&) = delete;
	IterAdapter& operator=(const IterAdapter&) = delete;
};

class IterNode : public ASTNode
{
public:
	std::string varName;
	ASTNodePtr collection;
	std::vector<IterAdapter> adapters;
	ASTNodePtr body;

	IterNode(std::string n, ASTNodePtr coll, std::vector<IterAdapter> iterAdapters, ASTNodePtr b)
		: varName(std::move(n))
		, collection(std::move(coll))
		, adapters(std::move(iterAdapters))
		, body(std::move(b))
	{
	}

	void Accept(ASTVisitor& v) override { v.Visit(*this); }
};
