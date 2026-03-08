#pragma once

#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct GuidedRule
{
	std::string lhs;
	std::vector<std::string> rhs;
	std::set<std::string> guides;
};

using Rules = std::vector<GuidedRule>;

class RulesBuilder
{
public:
	explicit RulesBuilder(std::string const& str);

	Rules BuildGuidedRules();

	[[nodiscard]] Rules GetRawRules() const { return m_rules; }
	[[nodiscard]] std::unordered_set<std::string> GetNonTerms() const { return m_nonTerms; }
	std::unordered_map<std::string, std::set<std::string>> GetFollows();

private:
	void Init(std::istream& is);
	std::set<std::string> GetFirst(std::vector<std::string> const& rhs);

	Rules m_rules;
	std::unordered_set<std::string> m_nonTerms;
	std::unordered_set<std::string> m_lexemes;
	std::unordered_map<std::string, std::set<std::string>> m_guides;
};