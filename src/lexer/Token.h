#pragma once

#include "../errors/Error.h"
#include <string>

enum class TokenType
{
	ERROR,
};

struct Token
{
	TokenType type;
	std::string value;
	size_t pos;
	size_t line;
	Error error = Error::NONE;
};

inline bool operator==(Token const& left, Token const& right)
{
	return left.type == right.type && left.value == right.value && left.pos == right.pos && left.error == right.error;
}