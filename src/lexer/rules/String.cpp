#include "String.h"

Token stringRule::ParseString(Reader& reader, size_t pos, size_t line)
{
	std::string value;
	value += reader.Get();

	while (!reader.EndOfFile())
	{
		char ch = reader.Get();
		value += ch;

		if (ch == '"')
		{
			if (value.size() >= 2 && value[value.size() - 2] == '\\')
			{
				continue;
			}
			return { TokenType::STRING_LITERAL, value, pos, line, Error::NONE };
		}

		if (ch == '\n')
		{
			return { TokenType::ERROR, value, pos, line, Error::UNCLOSED_STRING };
		}
		if (ch == '\\' && !reader.EndOfFile())
		{
			value += reader.Get();
		}

		return { TokenType::ERROR, value, pos, line, Error::UNCLOSED_STRING };
	}

	return {};
}