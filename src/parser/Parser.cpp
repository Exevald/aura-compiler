#include "Parser.h"

#include "ASTBuilder.h"
#include "RemapToken.h"

#include <algorithm>
#include <iostream>
#include <mutex>
#include <unordered_map>

namespace
{
std::shared_ptr<const SLRParser::ParserTables> BuildParserTables(const std::string& grammarText)
{
	auto tables = std::make_shared<SLRParser::ParserTables>();

	RulesBuilder builder(grammarText);
	builder.BuildGuidedRules();
	tables->rules = builder.GetRawRules();

	TableBuilder tableBuilder(tables->rules, builder.GetNonTerms(), builder.GetFollows());
	tableBuilder.Build();
	tables->actionTable = tableBuilder.GetActionTable();
	tables->gotoTable = tableBuilder.GetGotoTable();

	return tables;
}

std::shared_ptr<const SLRParser::ParserTables> GetCachedParserTables(const std::string& grammarText)
{
	static std::mutex cacheMutex;
	static std::unordered_map<std::string, std::shared_ptr<const SLRParser::ParserTables>> cache;

	std::scoped_lock lock(cacheMutex);
	if (const auto it = cache.find(grammarText); it != cache.end())
	{
		return it->second;
	}

	auto tables = BuildParserTables(grammarText);
	cache.emplace(grammarText, tables);
	return tables;
}
} // namespace

SLRParser::SLRParser(Lexer& lexer, const std::string& grammarText)
	: m_lexer(lexer)
	, m_tables(GetCachedParserTables(grammarText))
{
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

		if (m_tables->actionTable.contains(s) && m_tables->actionTable.at(s).contains(stringToken))
		{
			auto [type, value] = m_tables->actionTable.at(s).at(stringToken);

			if (type == ActionType::SHIFT)
			{
				m_semanticStack.push(std::make_unique<LeafNode>(stringToken, token.value));
				stateStack.push(value);
				token = m_lexer.Get();
			}
			else if (type == ActionType::REDUCE)
			{
				const auto& rule = m_tables->rules[value];
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
				stateStack.push(m_tables->gotoTable.at(top).at(rule.lhs));
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
