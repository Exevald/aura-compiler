#pragma once

#include "../Token.h"
#include "../reader/Reader.h"
#include <unordered_map>

class KeywordMap
{
public:
	KeywordMap();
	[[nodiscard]] TokenType Lookup(const std::string& id) const;

private:
	std::unordered_map<std::string, TokenType> m_map;
};

namespace identifierRule
{
Token ParseIdentifier(Reader& reader, size_t pos, size_t line, const KeywordMap& keywords);
}