#include "LexerTest.h"

TEST_F(LexerTest, KeywordModule)
{
	const Token token = GetToken("module");
	EXPECT_EQ(token.type, TokenType::KW_MODULE);
	EXPECT_EQ(token.value, "module");
}

TEST_F(LexerTest, KeywordFn)
{
	const Token token = GetToken("fn");
	EXPECT_EQ(token.type, TokenType::KW_FN);
}

TEST_F(LexerTest, KeywordStruct)
{
	const Token token = GetToken("struct");
	EXPECT_EQ(token.type, TokenType::KW_STRUCT);
}

TEST_F(LexerTest, KeywordIf)
{
	const Token token = GetToken("if");
	EXPECT_EQ(token.type, TokenType::KW_IF);
}

TEST_F(LexerTest, KeywordElse)
{
	const Token token = GetToken("else");
	EXPECT_EQ(token.type, TokenType::KW_ELSE);
}

TEST_F(LexerTest, KeywordWhile)
{
	const Token token = GetToken("while");
	EXPECT_EQ(token.type, TokenType::KW_WHILE);
}

TEST_F(LexerTest, KeywordReturn)
{
	const Token token = GetToken("return");
	EXPECT_EQ(token.type, TokenType::KW_RETURN);
}

TEST_F(LexerTest, KeywordTrue)
{
	const Token token = GetToken("true");
	EXPECT_EQ(token.type, TokenType::KW_TRUE);
}

TEST_F(LexerTest, KeywordFalse)
{
	const Token token = GetToken("false");
	EXPECT_EQ(token.type, TokenType::KW_FALSE);
}

TEST_F(LexerTest, KeywordConst)
{
	const Token token = GetToken("const");
	EXPECT_EQ(token.type, TokenType::KW_CONST);
}

TEST_F(LexerTest, KeywordVar)
{
	const Token token = GetToken("var");
	EXPECT_EQ(token.type, TokenType::KW_VAR);
}

TEST_F(LexerTest, KeywordType)
{
	const Token token = GetToken("type");
	EXPECT_EQ(token.type, TokenType::KW_TYPE);
}

TEST_F(LexerTest, KeywordEnum)
{
	const Token token = GetToken("enum");
	EXPECT_EQ(token.type, TokenType::KW_ENUM);
}

TEST_F(LexerTest, KeywordInterface)
{
	const Token token = GetToken("interface");
	EXPECT_EQ(token.type, TokenType::KW_INTERFACE);
}

TEST_F(LexerTest, KeywordImport)
{
	const Token token = GetToken("import");
	EXPECT_EQ(token.type, TokenType::KW_IMPORT);
}

TEST_F(LexerTest, KeywordExport)
{
	const Token token = GetToken("export");
	EXPECT_EQ(token.type, TokenType::KW_EXPORT);
}

TEST_F(LexerTest, KeywordActor)
{
	const Token token = GetToken("actor");
	EXPECT_EQ(token.type, TokenType::KW_ACTOR);
}

TEST_F(LexerTest, KeywordEffect)
{
	const Token token = GetToken("effect");
	EXPECT_EQ(token.type, TokenType::KW_EFFECT);
}

TEST_F(LexerTest, KeywordComptime)
{
	const Token token = GetToken("comptime");
	EXPECT_EQ(token.type, TokenType::KW_COMPTIME);
}

TEST_F(LexerTest, KeywordGo)
{
	const Token token = GetToken("go");
	EXPECT_EQ(token.type, TokenType::KW_GO);
}

TEST_F(LexerTest, KeywordAwait)
{
	const Token token = GetToken("await");
	EXPECT_EQ(token.type, TokenType::KW_AWAIT);
}
