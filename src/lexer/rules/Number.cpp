#include "Number.h"

Token numberRule::ParseNumber(Reader& reader, size_t pos, size_t line)
{
	std::string value;
	bool has_decimal = false;

	while (!reader.EndOfFile() && std::isdigit(static_cast<unsigned char>(reader.Peek())))
	{
		value += reader.Get();
	}

	if (!reader.EndOfFile() && reader.Peek() == '.' && reader.Peek() != EOF
		&& std::isdigit(static_cast<unsigned char>(reader.Peek())))
	{
		value += reader.Get();
		has_decimal = true;

		while (!reader.EndOfFile() && std::isdigit(static_cast<unsigned char>(reader.Peek())))
		{
			value += reader.Get();
		}
	}

	return {
		has_decimal ? TokenType::FLOAT_LITERAL : TokenType::INTEGER_LITERAL,
		value, pos, line, Error::NONE
	};
}
