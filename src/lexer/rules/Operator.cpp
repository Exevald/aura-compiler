#include "Operator.h"

Token operatorRule::ParseOperator(Reader& reader, size_t pos, size_t line)
{
	const char ch = reader.Get();

	if (!reader.EndOfFile())
	{
		const char next = reader.Peek();

		if (ch == '=' && next == '=')
		{
			reader.Get();
			return { TokenType::OP_EQUAL, "==", pos, line, Error::NONE };
		}
		if (ch == '!' && next == '=')
		{
			reader.Get();
			return { TokenType::OP_NOT_EQUAL, "!=", pos, line, Error::NONE };
		}
		if (ch == '<' && next == '=')
		{
			reader.Get();
			return { TokenType::OP_LESS_OR_EQUAL, "<=", pos, line, Error::NONE };
		}
		if (ch == '>' && next == '=')
		{
			reader.Get();
			return { TokenType::OP_GREATER_OR_EQUAL, ">=", pos, line, Error::NONE };
		}
		if (ch == '-' && next == '>')
		{
			reader.Get();
			return { TokenType::ARROW, "->", pos, line, Error::NONE };
		}
		if (ch == '&' && next == '&')
		{
			reader.Get();
			return { TokenType::OP_DOUBLE_AMPERSAND, "&&", pos, line, Error::NONE };
		}
		if (ch == '|' && next == '|')
		{
			reader.Get();
			return { TokenType::OP_DOUBLE_PIPE, "||", pos, line, Error::NONE };
		}
	}

	switch (ch)
	{
	case '(':
		return { TokenType::PARAN_OPEN, "(", pos, line, Error::NONE };
	case ')':
		return { TokenType::PARAN_CLOSE, ")", pos, line, Error::NONE };
	case '{':
		return { TokenType::CURLY_OPEN, "{", pos, line, Error::NONE };
	case '}':
		return { TokenType::CURLY_CLOSE, "}", pos, line, Error::NONE };
	case '[':
		return { TokenType::BRACKET_OPEN, "[", pos, line, Error::NONE };
	case ']':
		return { TokenType::BRACKET_CLOSE, "]", pos, line, Error::NONE };
	case ';':
		return { TokenType::SEMICOLON, ";", pos, line, Error::NONE };
	case ',':
		return { TokenType::COMMA, ",", pos, line, Error::NONE };
	case ':':
		return { TokenType::COLON, ":", pos, line, Error::NONE };
	case '.':
		return { TokenType::DOT, ".", pos, line, Error::NONE };
	case '|':
		return { TokenType::PIPE, "|", pos, line, Error::NONE };
	case '+':
		return { TokenType::OP_PLUS, "+", pos, line, Error::NONE };
	case '-':
		return { TokenType::OP_MINUS, "-", pos, line, Error::NONE };
	case '*':
		return { TokenType::OP_MUL, "*", pos, line, Error::NONE };
	case '/':
		return { TokenType::OP_DIVISION, "/", pos, line, Error::NONE };
	case '=':
		return { TokenType::OP_ASSIGNMENT, "=", pos, line, Error::NONE };
	case '<':
		return { TokenType::OP_LESS, "<", pos, line, Error::NONE };
	case '>':
		return { TokenType::OP_GREATER, ">", pos, line, Error::NONE };
	case '!':
		return { TokenType::ERROR, "!", pos, line, Error::UNEXPECTED_CHARACTER };
	default:
		return { TokenType::ERROR, std::string(1, ch), pos, line, Error::UNEXPECTED_CHARACTER };
	}
}