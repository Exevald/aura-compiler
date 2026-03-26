#include "Parser.h"

#include "ASTBuilder.h"
#include "RemapToken.h"

#include <algorithm>
#include <iostream>

SLRParser::SLRParser(Lexer& lexer, const std::string& grammarText)
	: m_lexer(lexer)
{
	RulesBuilder builder(grammarText);
	builder.BuildGuidedRules();
	m_rules = builder.GetRawRules();
	TableBuilder tableBuilder(m_rules, builder.GetNonTerms(), builder.GetFollows());
	tableBuilder.Build();
	m_actionTable = tableBuilder.GetActionTable();
	m_gotoTable = tableBuilder.GetGotoTable();
}

bool SLRParser::Parse()
{
	std::stack<int> stateStack;
	stateStack.push(0);
	Token token = m_lexer.Get();

	while (true)
	{
		int s = stateStack.top();
		const std::string stringToken = remapToken::RemapTokenTypeToString(token);

		if (m_actionTable[s].contains(stringToken))
		{
			auto [type, value] = m_actionTable[s][stringToken];

			if (type == ActionType::SHIFT)
			{
				m_semanticStack.push(std::make_unique<LeafNode>(stringToken, token.value));
				stateStack.push(value);
				token = m_lexer.Get();
			}
			else if (type == ActionType::REDUCE)
			{
				const auto& rule = m_rules[value];
				auto newNode = std::make_unique<RawNode>(rule.lhs);

				std::vector<ASTNodePtr> children;
				for (const auto& rh : rule.rhs)
				{
					if (rh != "EPSILON")
					{
						if (!m_semanticStack.empty())
						{
							children.push_back(std::move(m_semanticStack.top()));
							m_semanticStack.pop();
						}
						stateStack.pop();
					}
				}

				std::ranges::reverse(children);
				newNode->children = std::move(children);

				m_semanticStack.push(std::move(newNode));

				int top = stateStack.top();
				stateStack.push(m_gotoTable[top][rule.lhs]);
			}
			else if (type == ActionType::ACCEPT)
			{
				if (!m_semanticStack.empty())
				{
					ASTBuilder builder;
					m_root = builder.Build(std::move(m_semanticStack.top()));
				}
				return true;
			}
		}
		else
		{
			std::cerr << "Syntax Error at " << token.value << " line " << token.line << std::endl;
			return false;
		}
	}
}