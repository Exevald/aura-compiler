#include "Number.h"

Token numberRule::ParseNumber(Reader& reader, size_t pos, size_t line)
{
	std::string value;
	bool hasDecimal = false;

	while (!reader.EndOfFile() && std::isdigit(static_cast<unsigned char>(reader.Peek())))
	{
		value += reader.Get();
	}

	if (!reader.EndOfFile() && reader.Peek() == '.')
	{
		reader.Get();
		if (!reader.EndOfFile() && std::isdigit(static_cast<unsigned char>(reader.Peek())))
		{
			value += '.';
			hasDecimal = true;

			while (!reader.EndOfFile() && std::isdigit(static_cast<unsigned char>(reader.Peek())))
			{
				value += reader.Get();
			}
		}
		else
		{
			reader.Unget();
		}
	}

	return {
		hasDecimal ? TokenType::FLOAT_LITERAL : TokenType::INTEGER_LITERAL,
		value, pos, line, Error::NONE
	};
}