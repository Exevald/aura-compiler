#include "LexerTest.h"

TEST_F(LexerTest, SimpleIdentifier)
{
	const Token token = GetToken("variable");
	EXPECT_EQ(token.type, TokenType::ID);
	EXPECT_EQ(token.value, "variable");
	EXPECT_EQ(token.pos, 0);
	EXPECT_EQ(token.line, 0);
	EXPECT_EQ(token.error, Error::NONE);
}

TEST_F(LexerTest, IdentifierWithUnderscore)
{
	const Token token = GetToken("_myVar");
	EXPECT_EQ(token.type, TokenType::ID);
	EXPECT_EQ(token.value, "_myVar");
}

TEST_F(LexerTest, IdentifierWithNumbers)
{
	const Token token = GetToken("var123");
	EXPECT_EQ(token.type, TokenType::ID);
	EXPECT_EQ(token.value, "var123");
}

TEST_F(LexerTest, QualifiedIdentifier)
{
	const Token token = GetToken("std::vector");
	EXPECT_EQ(token.type, TokenType::ID);
	EXPECT_EQ(token.value, "std::vector");
}

TEST_F(LexerTest, QualifiedIdAsSeparateTokens)
{
	const std::vector<Token> tokens = GetAllTokens("std.io");

	EXPECT_EQ(tokens[0].type, TokenType::ID);
	EXPECT_EQ(tokens[0].value, "std");

	EXPECT_EQ(tokens[1].type, TokenType::DOT);

	EXPECT_EQ(tokens[2].type, TokenType::ID);
	EXPECT_EQ(tokens[2].value, "io");
}

TEST_F(LexerTest, IntegerLiteral)
{
	const Token token = GetToken("123");
	EXPECT_EQ(token.type, TokenType::INTEGER_LITERAL);
	EXPECT_EQ(token.value, "123");
}

TEST_F(LexerTest, IntegerLiteralZero)
{
	const Token token = GetToken("0");
	EXPECT_EQ(token.type, TokenType::INTEGER_LITERAL);
	EXPECT_EQ(token.value, "0");
}

TEST_F(LexerTest, FloatLiteral)
{
	const Token token = GetToken("3.14");
	EXPECT_EQ(token.type, TokenType::FLOAT_LITERAL);
	EXPECT_EQ(token.value, "3.14");
}

TEST_F(LexerTest, FloatLiteralZero)
{
	const Token token = GetToken("0.5");
	EXPECT_EQ(token.type, TokenType::FLOAT_LITERAL);
	EXPECT_EQ(token.value, "0.5");
}

TEST_F(LexerTest, SimpleString)
{
	const Token token = GetToken("\"hello\"");
	EXPECT_EQ(token.type, TokenType::STRING_LITERAL);
	EXPECT_EQ(token.value, "\"hello\"");
}

TEST_F(LexerTest, EmptyString)
{
	const Token token = GetToken("\"\"");
	EXPECT_EQ(token.type, TokenType::STRING_LITERAL);
	EXPECT_EQ(token.value, "\"\"");
}

TEST_F(LexerTest, StringWithEscape)
{
	const Token token = GetToken(R"("hello\nworld")");
	EXPECT_EQ(token.type, TokenType::STRING_LITERAL);
	EXPECT_EQ(token.value, "\"hello\\nworld\"");
}

TEST_F(LexerTest, StringWithQuoteEscape)
{
	const Token token = GetToken(R"("say \"hi\"")");
	EXPECT_EQ(token.type, TokenType::STRING_LITERAL);
	EXPECT_EQ(token.value, "\"say \\\"hi\\\"\"");
}

TEST_F(LexerTest, UnclosedString)
{
	const Token token = GetToken("\"unclosed");
	EXPECT_EQ(token.type, TokenType::ERROR);
	EXPECT_EQ(token.error, Error::UNCLOSED_STRING);
}

TEST_F(LexerTest, StringWithNewline)
{
	const Token token = GetToken("\"line1\nline2\"");
	EXPECT_EQ(token.type, TokenType::ERROR);
	EXPECT_EQ(token.error, Error::UNCLOSED_STRING);
}