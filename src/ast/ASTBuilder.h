#pragma once
#include "AST.h"
#include <map>

class ASTBuilder
{
public:
	ASTNodePtr Build(ASTNodePtr node);

private:
	ASTNodePtr Simplify(ASTNodePtr node);

	static void FlattenParams(const RawNode* raw, std::vector<Parameter>& target);
	void FlattenArgs(RawNode* raw, std::vector<ASTNodePtr>& target);
};