#include "LexerTest.h"

TEST_F(LexerTest, SkipSpaces)
{
	const Token token = GetToken("   variable");
	EXPECT_EQ(token.type, TokenType::ID);
	EXPECT_EQ(token.value, "variable");
}

TEST_F(LexerTest, SkipTabs)
{
	const Token token = GetToken("\t\tvariable");
	EXPECT_EQ(token.type, TokenType::ID);
}

TEST_F(LexerTest, SkipNewlines)
{
	const Token token = GetToken("\n\nvariable");
	EXPECT_EQ(token.type, TokenType::ID);
	EXPECT_EQ(token.line, 2);
}

TEST_F(LexerTest, SkipLineComment)
{
	const Token token = GetToken("// comment\nvariable");
	EXPECT_EQ(token.type, TokenType::ID);
	EXPECT_EQ(token.line, 1);
}

TEST_F(LexerTest, SkipBlockComment)
{
	const Token token = GetToken("/* comment */variable");
	EXPECT_EQ(token.type, TokenType::ID);
}

TEST_F(LexerTest, SkipBlockCommentMultiline)
{
	const Token token = GetToken("/* line1\nline2 */variable");
	EXPECT_EQ(token.type, TokenType::ID);
	EXPECT_EQ(token.line, 1);
}

TEST_F(LexerTest, SkipMultipleComments)
{
	const Token token = GetToken("// comment1\n// comment2\nvariable");
	EXPECT_EQ(token.type, TokenType::ID);
	EXPECT_EQ(token.line, 2);
}

TEST_F(LexerTest, EmptyInput)
{
	Lexer lexer("");
	EXPECT_TRUE(lexer.Empty());
	const Token token = lexer.Get();
	EXPECT_EQ(token.type, TokenType::EOF_TOKEN);
}

TEST_F(LexerTest, OnlyWhitespace)
{
	Lexer lexer("   \n\t  ");
	EXPECT_TRUE(lexer.Empty());
	const Token token = lexer.Get();
	EXPECT_EQ(token.type, TokenType::EOF_TOKEN);
}

TEST_F(LexerTest, EOFAfterTokens)
{
	const std::vector<Token> tokens = GetAllTokens("x");
	EXPECT_EQ(tokens.size(), 2);
	EXPECT_EQ(tokens[0].type, TokenType::ID);
	EXPECT_EQ(tokens[1].type, TokenType::EOF_TOKEN);
}

TEST_F(LexerTest, PositionTracking)
{
	Lexer lexer("abc");
	Token token = lexer.Get();
	EXPECT_EQ(token.pos, 0);
	token = lexer.Get();
	EXPECT_EQ(token.type, TokenType::EOF_TOKEN);
}

TEST_F(LexerTest, PositionAfterWhitespace)
{
	Lexer lexer("  abc");
	const Token token = lexer.Get();
	EXPECT_EQ(token.pos, 2);
}

TEST_F(LexerTest, LineTracking)
{
	Lexer lexer("a\nb\nc");
	Token token = lexer.Get();
	EXPECT_EQ(token.line, 0);
	EXPECT_EQ(token.value, "a");

	token = lexer.Get();
	EXPECT_EQ(token.line, 1);
	EXPECT_EQ(token.value, "b");

	token = lexer.Get();
	EXPECT_EQ(token.line, 2);
	EXPECT_EQ(token.value, "c");
}

// ==================== PEEK ====================

TEST_F(LexerTest, PeekDoesNotConsume)
{
	Lexer lexer("abc");
	Token token1 = lexer.Peek();
	Token token2 = lexer.Get();
	EXPECT_EQ(token1.type, token2.type);
	EXPECT_EQ(token1.value, token2.value);
}

TEST_F(LexerTest, PeekOnEmpty)
{
	Lexer lexer("");
	Token token = lexer.Peek();
	EXPECT_EQ(token.type, TokenType::ERROR);
	EXPECT_EQ(token.error, Error::EMPTY_INPUT);
}
