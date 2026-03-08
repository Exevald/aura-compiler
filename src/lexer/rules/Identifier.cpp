#include "Identifier.h"

Token identifierRule::ParseIdentifier(Reader& reader, size_t pos, size_t line, const KeywordMap& keywords)
{
	std::string value;

	if (const auto first = reader.Peek();
		!std::isalpha(static_cast<unsigned char>(first)) && first != '_')
	{
		return {
			TokenType::ERROR,
			"",
			pos,
			line,
			Error::UNEXPECTED_CHARACTER
		};
	}

	value += reader.Get();
	while (!reader.EndOfFile())
	{
		if (const char ch = reader.Peek();
			std::isalnum(static_cast<unsigned char>(ch)) || ch == '_')
		{
			value += reader.Get();
		}
		else
		{
			break;
		}
	}

	while (!reader.EndOfFile() && reader.Peek() == ':')
	{
		reader.Get();

		if (reader.EndOfFile() || reader.Peek() != ':')
		{
			reader.Unget();
			break;
		}

		reader.Get();
		value += "::";

		if (reader.EndOfFile())
		{
			break;
		}

		if (const char next = reader.Peek();
			!std::isalpha(static_cast<unsigned char>(next)) && next != '_')
		{
			break;
		}

		value += reader.Get();

		while (!reader.EndOfFile())
		{
			if (const char ch = reader.Peek();
				std::isalnum(static_cast<unsigned char>(ch)) || ch == '_')
			{
				value += reader.Get();
			}
			else
			{
				break;
			}
		}
	}

	if (value.find("::") == std::string::npos)
	{
		if (const TokenType kwType = keywords.Lookup(value);
			kwType != TokenType::ID)
		{
			return { kwType, value, pos, line, Error::NONE };
		}
	}

	return { TokenType::ID, value, pos, line, Error::NONE };
}