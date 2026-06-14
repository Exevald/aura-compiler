#pragma once

#include "AST.h"
#include "ASTSearcher.h"
#include "Lexer.h"
#include "OpCode.h"
#include "Parser.h"
#include "Utils.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <string>

using namespace VM::Core;

class ASTTest : public ::testing::Test
{
protected:
	static ASTNodePtr ParseCode(const std::string& source)
	{
		std::ifstream file(GrammarPath());
		if (!file.is_open())
		{
			return nullptr;
		}

		Lexer lexer(source);
		SLRParser parser(lexer, NormalizeGrammar(file));
		if (!parser.Parse())
		{
			return nullptr;
		}
		return parser.GetRoot();
	}

	template <typename T>
	static bool HasNode(ASTNode* root)
	{
		ASTSearcher searcher;
		searcher.targetType = &typeid(T);
		if (root)
		{
			root->Accept(searcher);
		}
		return searcher.foundType;
	}

	static bool HasRule(ASTNode* root, const std::string& ruleName)
	{
		ASTSearcher searcher;
		searcher.targetRule = ruleName;
		if (root)
		{
			root->Accept(searcher);
		}
		return searcher.foundRule;
	}

	static bool HasLeaf(ASTNode* root, const std::string& value)
	{
		ASTSearcher searcher;
		searcher.targetValue = value;
		if (root)
		{
			root->Accept(searcher);
		}
		return searcher.foundValue;
	}
};
