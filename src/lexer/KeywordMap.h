#pragma once

#include "Token.h"

#include <map>
#include <string>

class KeywordMap
{
public:
	KeywordMap();
	[[nodiscard]] TokenType Lookup(const std::string& id) const;

private:
	std::map<std::string, TokenType> m_map;
};