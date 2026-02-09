#include "Lexer.h"
#include "rules/Identifier.h"
#include "rules/Number.h"
#include "rules/Operator.h"
#include "rules/String.h"
#include "rules/Whitespace.h"

class Lexer::KeywordMap : public ::KeywordMap
{
};

Lexer::Lexer(std::string const& input)
	: m_reader(input)
	, m_keywords(new KeywordMap())
{
}

Token Lexer::Get()
{
	SkipWhitespaces();

	if (m_reader.EndOfFile())
	{
		return {
			TokenType::EOF_TOKEN,
			/*value=*/"",
			m_reader.Count(),
			m_reader.LineCount(),
			Error::NONE
		};
	}

	char ch = m_reader.Peek();
	if (std::isalpha(static_cast<unsigned char>(ch)) || ch == '_')
	{
		return Id();
	}
	if (std::isdigit(static_cast<unsigned char>(ch)))
	{
		return Number();
	}

	if (ch == '"')
		return String();

	return SpecialChar();
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
	size_t pos = m_reader.Count();
	Token token = Get();
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
	size_t pos = m_reader.Count();
	size_t line = m_reader.LineCount();
	return identifierRule::ParseIdentifier(m_reader, pos, line, *m_keywords);
}

Token Lexer::Number()
{
	size_t pos = m_reader.Count();
	size_t line = m_reader.LineCount();
	return numberRule::ParseNumber(m_reader, pos, line);
}

Token Lexer::String()
{
	size_t pos = m_reader.Count();
	size_t line = m_reader.LineCount();
	return stringRule::ParseString(m_reader, pos, line);
}

Token Lexer::SpecialChar()
{
	size_t pos = m_reader.Count();
	size_t line = m_reader.LineCount();
	return operatorRule::ParseOperator(m_reader, pos, line);
}

void Lexer::SkipWhitespaces()
{
	whitespaceRule::SkipWhitespacesAndComments(m_reader);
}
