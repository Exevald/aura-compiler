#include "Parser.h"
#include "RemapToken.h"

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
		std::string a = remapToken::RemapTokenTypeToString(token);
		if (m_actionTable[s].count(a))
		{
			Action action = m_actionTable[s][a];

			if (action.type == ActionType::SHIFT)
			{
				stack.push(action.value);
				token = m_lexer.Get();
			}
			else if (action.type == ActionType::REDUCE)
			{
				const auto& rule = m_rules[action.value];
				size_t symbolsToPop = rule.rhs.size();
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

				int top = stack.top();
				if (m_gotoTable.count(top) && m_gotoTable[top].count(rule.lhs))
				{
					stack.push(m_gotoTable[top][rule.lhs]);
				}
				else
				{
					std::cerr << "Syntax error: no GOTO entry for " << rule.lhs << " from state " << top << "\n";
					return false;
				}
			}
			else if (action.type == ActionType::ACCEPT)
			{
				return true;
			}
		}
		else
		{
			std::cerr << "Syntax error at " << token.value << " (line " << token.line << ")\n";
			return false;
		}
	}
}
