#pragma once

#include "KeywordMap.h"
#include "Token.h"
#include "reader/Reader.h"

class Lexer
{
public:
	explicit Lexer(std::string const& input);
	~Lexer();

	Token Get();
	Token Peek();
	[[nodiscard]] bool Empty();

private:
	Token Id();
	Token Number();
	Token String();
	Token SpecialChar();
	void SkipWhitespaces();

	Reader m_reader;
	KeywordMap* m_keywords;
};