#include "RulesBuilder.h"

#include <set>
#include <sstream>

RulesBuilder::RulesBuilder(std::string const& str)
{
	std::stringstream ss(str);
	Init(ss);
}

void RulesBuilder::Init(std::istream& is)
{
	const std::string content((std::istreambuf_iterator(is)), std::istreambuf_iterator<char>());
	m_rules.push_back({ "_S_PRIME_", { "program" } });
	m_nonTerms.insert("_S_PRIME_");

	std::stringstream ss(content);
	std::string word;
	std::string currentLhs;
	std::vector<std::string> currentRhs;
	bool expectEq = false;

	while (ss >> word)
	{
		if (currentLhs.empty())
		{
			currentLhs = word;
			m_nonTerms.insert(currentLhs);
			expectEq = true;
			continue;
		}
		if (expectEq)
		{
			if (word == "=")
			{
				expectEq = false;
			}
			continue;
		}

		if (word == ";")
		{
			m_rules.push_back({ currentLhs, currentRhs });
			currentRhs.clear();
			currentLhs.clear();
		}
		else if (word == "|")
		{
			m_rules.push_back({ currentLhs, currentRhs });
			currentRhs.clear();
		}
		else if (word == "EPSILON")
		{
		}
		else
		{
			if (word.size() >= 2 && word.front() == '"' && word.back() == '"')
			{
				word = word.substr(1, word.size() - 2);
			}
			currentRhs.push_back(word);
		}
	}

	for (auto& rule : m_rules)
	{
		for (auto& sym : rule.rhs)
		{
			if (!m_nonTerms.contains(sym))
			{
				m_lexemes.insert(sym);
			}
		}
	}
}

std::set<std::string> RulesBuilder::GetFirst(std::vector<std::string> const& rhs)
{
	std::set<std::string> first;

	if (rhs.empty())
	{
		first.insert("EPSILON");
		return first;
	}

	for (size_t i = 0; i < rhs.size(); ++i)
	{
		const std::string& sym = rhs[i];
		if (m_lexemes.contains(sym))
		{
			first.insert(sym);
			return first;
		}
		auto const& symFirst = m_guides[sym];
		bool hasEpsilon = false;
		for (auto const& f : symFirst)
		{
			if (f == "EPSILON")
			{
				hasEpsilon = true;
			}
			else
			{
				first.insert(f);
			}
		}
		if (!hasEpsilon)
		{
			return first;
		}
		if (i == rhs.size() - 1)
		{
			first.insert("EPSILON");
		}
	}
	return first;
}

Rules RulesBuilder::BuildGuidedRules()
{
	for (auto const& nt : m_nonTerms)
		m_guides[nt] = {};

	bool changed = true;
	while (changed)
	{
		changed = false;
		for (auto const& r : m_rules)
		{
			auto first = GetFirst(r.rhs);
			size_t oldSize = m_guides[r.lhs].size();
			m_guides[r.lhs].insert(first.begin(), first.end());
			if (m_guides[r.lhs].size() > oldSize)
			{
				changed = true;
			}
		}
	}

	std::unordered_map<std::string, std::set<std::string>> follows;
	follows["program"].insert("EOF");
	changed = true;
	while (changed)
	{
		changed = false;
		for (auto const& r : m_rules)
		{
			for (size_t i = 0; i < r.rhs.size(); ++i)
			{
				if (m_nonTerms.contains(r.rhs[i]))
				{
					std::vector<std::string> rest(r.rhs.begin() + i + 1, r.rhs.end());
					auto firstRest = GetFirst(rest);

					const size_t oldSize = follows[r.rhs[i]].size();
					for (auto const& f : firstRest)
					{
						if (f != "EPSILON")
						{
							follows[r.rhs[i]].insert(f);
						}
					}
					if (firstRest.contains("EPSILON") || rest.empty())
					{
						follows[r.rhs[i]].insert(follows[r.lhs].begin(), follows[r.lhs].end());
					}
					if (follows[r.rhs[i]].size() > oldSize)
					{
						changed = true;
					}
				}
			}
		}
	}

	Rules result;
	for (auto const& r : m_rules)
	{
		GuidedRule rule;
		rule.lhs = r.lhs;
		rule.rhs = r.rhs;

		for (auto first = GetFirst(r.rhs); auto const& f : first)
		{
			if (f != "EPSILON")
				rule.guides.insert(f);
			else
			{
				rule.guides.insert(follows[r.lhs].begin(), follows[r.lhs].end());
			}
		}
		result.push_back(rule);
	}
	return result;
}

std::unordered_map<std::string, std::set<std::string>> RulesBuilder::GetFollows()
{
	std::unordered_map<std::string, std::set<std::string>> follows;
	for (const auto& nt : m_nonTerms)
	{
		follows[nt] = {};
	}

	follows["_S_PRIME_"].insert("EOF");

	bool changed = true;
	while (changed)
	{
		changed = false;
		for (const auto& rule : m_rules)
		{
			for (size_t i = 0; i < rule.rhs.size(); ++i)
			{
				if (m_nonTerms.contains(rule.rhs[i]))
				{
					const std::string& B = rule.rhs[i];
					std::vector rest(rule.rhs.begin() + i + 1, rule.rhs.end());
					auto firstRest = GetFirst(rest);

					const size_t oldSize = follows[B].size();
					for (const auto& f : firstRest)
					{
						if (f != "EPSILON")
						{
							follows[B].insert(f);
						}
					}

					if (firstRest.contains("EPSILON") || (i + 1 == rule.rhs.size()))
					{
						follows[B].insert(follows[rule.lhs].begin(), follows[rule.lhs].end());
					}
					if (follows[B].size() > oldSize)
					{
						changed = true;
					}
				}
			}
		}
	}
	return follows;
}