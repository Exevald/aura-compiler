#pragma once

#include "../rulesBuilder/RulesBuilder.h"
#include "Lexer.h"
#include "TableBuilder.h"

class SLRParser
{
public:
	SLRParser(Lexer& lexer, const std::string& grammarText);

	bool Parse();

private:
	Lexer& m_lexer;
	Rules m_rules;
	std::map<int, std::map<std::string, Action>> m_actionTable;
	std::map<int, std::map<std::string, int>> m_gotoTable;
};