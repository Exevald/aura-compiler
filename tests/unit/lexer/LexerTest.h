#pragma once

#include "Lexer.h"
#include "Token.h"

#include <gtest/gtest.h>

class LexerTest : public ::testing::Test
{
protected:
	static Token GetToken(const std::string& input)
	{
		Lexer lexer(input);
		return lexer.Get();
	}

	static std::vector<Token> GetAllTokens(const std::string& input)
	{
		Lexer lexer(input);
		std::vector<Token> tokens;
		while (!lexer.Empty())
		{
			tokens.push_back(lexer.Get());
		}
		tokens.push_back(lexer.Get());
		return tokens;
	}
};