#pragma once

#include "../ast/AST.h"
#include "../rulesBuilder/RulesBuilder.h"
#include "../lexer/Lexer.h"
#include "TableBuilder.h"

#include <stack>

class SLRParser
{
public:
	SLRParser(Lexer& lexer, const std::string& grammarText);

	bool Parse();
	ASTNodePtr GetRoot() { return std::move(m_root); }

private:
	Lexer& m_lexer;
	Rules m_rules;
	std::map<int, std::map<std::string, Action>> m_actionTable;
	std::map<int, std::map<std::string, int>> m_gotoTable;

	std::stack<ASTNodePtr> m_semanticStack;
	ASTNodePtr m_root;
};