#include "Lexer.h"

Lexer::Lexer(std::string const& input)
	: m_reader(input)
{
}

Token Lexer::Get()
{
	return {};
}

Token Lexer::Peek()
{
	if (Empty())
	{
		return Token{
			.type = TokenType::ERROR,
			.pos = m_reader.Count(),
			.line = m_reader.LineCount(),
			.error = Error::EMPTY_INPUT
		};
	}
	const auto pos = m_reader.Count();
	const auto token = Get();
	m_reader.Seek(pos);

	return token;
}

bool Lexer::Empty()
{
	SkipWhitespaces();
	return m_reader.EndOfFile();
}

Token Lexer::Id()
{
	return {};
}

Token Lexer::Number()
{
	return {};
}

Token Lexer::String()
{
	return {};
}

Token Lexer::SpecialChar()
{
	return {};
}

void Lexer::SkipWhitespaces()
{
	while (!m_reader.EndOfFile() && (m_reader.Peek() == ' ' || m_reader.Peek() == '\n'))
	{
		m_reader.Get();
	}
}
