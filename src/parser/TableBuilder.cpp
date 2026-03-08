#include "TableBuilder.h"

#include <iostream>

TableBuilder::TableBuilder(
	Rules rules,
	const std::unordered_set<std::string>& nonTerms,
	const std::unordered_map<std::string,
		std::set<std::string>>& follows)
	: m_rules(std::move(rules))
	, m_nonTerms(nonTerms)
	, m_follows(follows)
{
}

void TableBuilder::Build()
{
	m_states.push_back(Closure({ { 0, 0 } }));

	for (int i = 0; i < m_states.size(); ++i)
	{
		for (const std::string& sym : GetPossibleSymbols(m_states[i]))
		{
			LR0State next = GoTo(m_states[i], sym);
			if (next.empty())
				continue;
			int nextIdx = GetOrAddState(next);

			if (m_nonTerms.count(sym))
			{
				m_gotoTable[i][sym] = nextIdx;
			}
			else
			{
				m_actionTable[i][sym] = { ActionType::SHIFT, nextIdx };
			}
		}

		for (const auto& item : m_states[i])
		{
			const auto& rule = m_rules[item.ruleIndex];
			if (item.dotPosition == rule.rhs.size())
			{
				if (rule.lhs == "_S_PRIME_")
				{
					m_actionTable[i]["EOF"] = { ActionType::ACCEPT, 0 };
				}
				else
				{
					const auto& followLhs = m_follows.at(rule.lhs);
					for (const std::string& f : followLhs)
					{
						if (m_actionTable[i].count(f))
						{
							if (m_actionTable[i][f].type == ActionType::SHIFT)
							{
								continue;
							}
						}
						m_actionTable[i][f] = { ActionType::REDUCE, item.ruleIndex };
					}
				}
			}
		}
	}
}

LR0State TableBuilder::Closure(LR0State state) const
{
	LR0State resultState = std::move(state);
	bool changed = true;
	while (changed)
	{
		changed = false;
		LR0State current = resultState;
		for (const auto& item : current)
		{
			const auto& rule = m_rules[item.ruleIndex];
			if (item.dotPosition < rule.rhs.size())
			{
				if (std::string nextSymbol = rule.rhs[item.dotPosition];
					m_nonTerms.contains(nextSymbol))
				{
					for (size_t k = 0; k < m_rules.size(); ++k)
					{
						if (m_rules[k].lhs == nextSymbol)
						{
							if (resultState.insert({ (int)k, 0 }).second)
							{
								changed = true;
							}
						}
					}
				}
			}
		}
	}
	return resultState;
}

LR0State TableBuilder::GoTo(const LR0State& state, const std::string& str) const
{
	LR0State resultState;
	for (const auto& [ruleIndex, dotPosition] : state)
	{
		if (const auto& rule = m_rules[ruleIndex];
			dotPosition < rule.rhs.size() && rule.rhs[dotPosition] == str)
		{
			resultState.insert({ ruleIndex, dotPosition + 1 });
		}
	}
	return Closure(resultState);
}

int TableBuilder::GetOrAddState(const LR0State& state)
{
	for (int i = 0; i < m_states.size(); ++i)
	{
		if (m_states[i] == state)
		{
			return i;
		}
	}
	m_states.push_back(state);
	return static_cast<int>(m_states.size() - 1);
}

std::set<std::string> TableBuilder::GetPossibleSymbols(const LR0State& state) const
{
	std::set<std::string> res;
	for (const auto& [ruleIndex, dotPosition] : state)
	{
		if (const auto& rule = m_rules[ruleIndex]; dotPosition < rule.rhs.size())
		{
			res.insert(rule.rhs[dotPosition]);
		}
	}
	return res;
}