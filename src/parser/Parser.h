#pragma once

#include "../ast/AST.h"
#include "../rulesBuilder/RulesBuilder.h"
#include "../lexer/Lexer.h"
#include "TableBuilder.h"

#include <memory>
#include <stack>

class SLRParser
{
public:
	struct ParserTables
	{
		Rules rules;
		std::map<int, std::map<std::string, Action>> actionTable;
		std::map<int, std::map<std::string, int>> gotoTable;
	};

	SLRParser(Lexer& lexer, const std::string& grammarText);

	bool Parse();
	ASTNodePtr GetRoot() { return std::move(m_root); }

private:
	Lexer& m_lexer;
	std::shared_ptr<const ParserTables> m_tables;

	std::stack<ASTNodePtr> m_semanticStack;
	ASTNodePtr m_root;
};
