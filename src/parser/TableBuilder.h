#pragma once

#include "../rulesBuilder/RulesBuilder.h"
#include "ParserTypes.h"

#include <map>

class TableBuilder
{
public:
	TableBuilder(Rules rules,
		const std::unordered_set<std::string>& nonTerms,
		const std::unordered_map<std::string, std::set<std::string>>& follows);

	void Build();

	std::map<int, std::map<std::string, Action>> GetActionTable() { return m_actionTable; }
	std::map<int, std::map<std::string, int>> GetGotoTable() { return m_gotoTable; }

private:
	LR0State Closure(LR0State state) const;
	LR0State GoTo(const LR0State& state, const std::string& str) const;
	int GetOrAddState(const LR0State& state);
	std::set<std::string> GetPossibleSymbols(const LR0State& state) const;

	std::vector<LR0State> m_states;
	std::unordered_set<std::string> m_nonTerms;
	std::unordered_map<std::string, std::set<std::string>> m_follows;
	std::map<int, std::map<std::string, Action>> m_actionTable;
	std::map<int, std::map<std::string, int>> m_gotoTable;
	Rules m_rules;
};