#include "Parser.h"
#include "RemapToken.h"

#include <iostream>
#include <stack>

SLRParser::SLRParser(Lexer& lexer, const std::string& grammarText)
	: m_lexer(lexer)
{
	RulesBuilder builder(grammarText);
	auto guided = builder.BuildGuidedRules();

	m_rules = builder.GetRawRules();
	TableBuilder tableBuilder(m_rules, builder.GetNonTerms(), builder.GetFollows());
	tableBuilder.Build();

	m_actionTable = tableBuilder.GetActionTable();
	m_gotoTable = tableBuilder.GetGotoTable();
}

bool SLRParser::Parse()
{
	std::stack<int> stack;
	stack.push(0);

	Token token = m_lexer.Get();

	while (true)
	{
		int s = stack.top();
		if (std::string a = remapToken::RemapTokenTypeToString(token); m_actionTable[s].contains(a))
		{

			if (auto [type, value] = m_actionTable[s][a]; type == ActionType::SHIFT)
			{
				stack.push(value);
				token = m_lexer.Get();
			}
			else if (type == ActionType::REDUCE)
			{
				const auto& rule = m_rules[value];
				const size_t symbolsToPop = rule.rhs.size();
				for (size_t i = 0; i < symbolsToPop; ++i)
				{
					if (!stack.empty())
					{
						stack.pop();
					}
					else
					{
						std::cerr << "Fatal error: Stack underflow during reduction\n";
						return false;
					}
				}

				if (int top = stack.top();
					m_gotoTable.contains(top) && m_gotoTable[top].contains(rule.lhs))
				{
					stack.push(m_gotoTable[top][rule.lhs]);
				}
				else
				{
					std::cerr << "Syntax error: no GOTO entry for "
							  << rule.lhs << " from state " << top << "\n";
					return false;
				}
			}
			else if (type == ActionType::ACCEPT)
			{
				return true;
			}
		}
		else
		{
			std::cerr << "Syntax error at "
					  << token.value << " (line " << token.line << ")\n";
			return false;
		}
	}
}
