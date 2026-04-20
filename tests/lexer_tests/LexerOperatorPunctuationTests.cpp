#include "LexerTest.h"

TEST_F(LexerTest, OperatorPlus)
{
	Token token = GetToken("+");
	EXPECT_EQ(token.type, TokenType::OP_PLUS);
	EXPECT_EQ(token.value, "+");
}

TEST_F(LexerTest, OperatorMinus)
{
	Token token = GetToken("-");
	EXPECT_EQ(token.type, TokenType::OP_MINUS);
}

TEST_F(LexerTest, OperatorMul)
{
	Token token = GetToken("*");
	EXPECT_EQ(token.type, TokenType::OP_MUL);
}

TEST_F(LexerTest, OperatorDivision)
{
	Token token = GetToken("/");
	EXPECT_EQ(token.type, TokenType::OP_DIVISION);
}

TEST_F(LexerTest, OperatorAssignment)
{
	Token token = GetToken("=");
	EXPECT_EQ(token.type, TokenType::OP_ASSIGNMENT);
}

TEST_F(LexerTest, OperatorEqual)
{
	Token token = GetToken("==");
	EXPECT_EQ(token.type, TokenType::OP_EQUAL);
	EXPECT_EQ(token.value, "==");
}

TEST_F(LexerTest, OperatorNotEqual)
{
	Token token = GetToken("!=");
	EXPECT_EQ(token.type, TokenType::OP_NOT_EQUAL);
	EXPECT_EQ(token.value, "!=");
}

TEST_F(LexerTest, OperatorLess)
{
	Token token = GetToken("<");
	EXPECT_EQ(token.type, TokenType::OP_LESS);
}

TEST_F(LexerTest, OperatorGreater)
{
	Token token = GetToken(">");
	EXPECT_EQ(token.type, TokenType::OP_GREATER);
}

TEST_F(LexerTest, OperatorLessOrEqual)
{
	Token token = GetToken("<=");
	EXPECT_EQ(token.type, TokenType::OP_LESS_OR_EQUAL);
}

TEST_F(LexerTest, OperatorGreaterOrEqual)
{
	Token token = GetToken(">=");
	EXPECT_EQ(token.type, TokenType::OP_GREATER_OR_EQUAL);
}

TEST_F(LexerTest, OperatorArrow)
{
	Token token = GetToken("->");
	EXPECT_EQ(token.type, TokenType::ARROW);
	EXPECT_EQ(token.value, "->");
}

TEST_F(LexerTest, OperatorDoubleAmpersand)
{
	Token token = GetToken("&&");
	EXPECT_EQ(token.type, TokenType::OP_DOUBLE_AMPERSAND);
}

TEST_F(LexerTest, OperatorAmpersand)
{
	Token token = GetToken("&");
	EXPECT_EQ(token.type, TokenType::OP_AMPERSAND);
	EXPECT_EQ(token.value, "&");
}

TEST_F(LexerTest, OperatorDoublePipe)
{
	Token token = GetToken("||");
	EXPECT_EQ(token.type, TokenType::OP_DOUBLE_PIPE);
}

TEST_F(LexerTest, ParanOpen)
{
	Token token = GetToken("(");
	EXPECT_EQ(token.type, TokenType::PARAN_OPEN);
}

TEST_F(LexerTest, ParanClose)
{
	Token token = GetToken(")");
	EXPECT_EQ(token.type, TokenType::PARAN_CLOSE);
}

TEST_F(LexerTest, CurlyOpen)
{
	Token token = GetToken("{");
	EXPECT_EQ(token.type, TokenType::CURLY_OPEN);
}

TEST_F(LexerTest, CurlyClose)
{
	Token token = GetToken("}");
	EXPECT_EQ(token.type, TokenType::CURLY_CLOSE);
}

TEST_F(LexerTest, BracketOpen)
{
	Token token = GetToken("[");
	EXPECT_EQ(token.type, TokenType::BRACKET_OPEN);
}

TEST_F(LexerTest, BracketClose)
{
	Token token = GetToken("]");
	EXPECT_EQ(token.type, TokenType::BRACKET_CLOSE);
}

TEST_F(LexerTest, Semicolon)
{
	Token token = GetToken(";");
	EXPECT_EQ(token.type, TokenType::SEMICOLON);
}

TEST_F(LexerTest, Comma)
{
	Token token = GetToken(",");
	EXPECT_EQ(token.type, TokenType::COMMA);
}

TEST_F(LexerTest, Colon)
{
	Token token = GetToken(":");
	EXPECT_EQ(token.type, TokenType::COLON);
}

TEST_F(LexerTest, Dot)
{
	Token token = GetToken(".");
	EXPECT_EQ(token.type, TokenType::DOT);
}

TEST_F(LexerTest, Pipe)
{
	Token token = GetToken("|");
	EXPECT_EQ(token.type, TokenType::PIPE);
}
