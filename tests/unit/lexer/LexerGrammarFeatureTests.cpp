#include "LexerTest.h"

TEST_F(LexerTest, FunctionDeclaration)
{
	std::vector<Token> tokens = GetAllTokens("fn add(x: int, y: int) -> int { return x + y; }");

	EXPECT_EQ(tokens[0].type, TokenType::KW_FN);
	EXPECT_EQ(tokens[1].type, TokenType::ID);
	EXPECT_EQ(tokens[1].value, "add");
	EXPECT_EQ(tokens[2].type, TokenType::PARAN_OPEN);
	EXPECT_EQ(tokens[3].type, TokenType::ID);
	EXPECT_EQ(tokens[3].value, "x");
	EXPECT_EQ(tokens[4].type, TokenType::COLON);
	EXPECT_EQ(tokens[5].type, TokenType::KW_INT);
}

TEST_F(LexerTest, StructDeclaration)
{
	std::vector<Token> tokens = GetAllTokens("struct Point { x: float; y: float; }");

	EXPECT_EQ(tokens[0].type, TokenType::KW_STRUCT);
	EXPECT_EQ(tokens[1].type, TokenType::ID);
	EXPECT_EQ(tokens[1].value, "Point");
	EXPECT_EQ(tokens[2].type, TokenType::CURLY_OPEN);
}

TEST_F(LexerTest, IfStatement)
{
	std::vector<Token> tokens = GetAllTokens("if (x > 0) { return x; } else { return 0; }");

	EXPECT_EQ(tokens[0].type, TokenType::KW_IF);
	EXPECT_EQ(tokens[1].type, TokenType::PARAN_OPEN);
	EXPECT_EQ(tokens[2].type, TokenType::ID);
	EXPECT_EQ(tokens[3].type, TokenType::OP_GREATER);
	EXPECT_EQ(tokens[4].type, TokenType::INTEGER_LITERAL);
}

TEST_F(LexerTest, ImportStatement)
{
	std::vector<Token> tokens = GetAllTokens("import std.io as io;");

	EXPECT_EQ(tokens[0].type, TokenType::KW_IMPORT);
	EXPECT_EQ(tokens[1].type, TokenType::ID);
	EXPECT_EQ(tokens[1].value, "std");
	EXPECT_EQ(tokens[2].type, TokenType::DOT);
	EXPECT_EQ(tokens[3].type, TokenType::ID);
	EXPECT_EQ(tokens[3].value, "io");
	EXPECT_EQ(tokens[4].type, TokenType::KW_AS);
	EXPECT_EQ(tokens[5].type, TokenType::ID);
	EXPECT_EQ(tokens[5].value, "io");
	EXPECT_EQ(tokens[6].type, TokenType::SEMICOLON);
}

TEST_F(LexerTest, ModuleDeclaration)
{
	std::vector<Token> tokens = GetAllTokens("module myapp.utils;");

	EXPECT_EQ(tokens[0].type, TokenType::KW_MODULE);
	EXPECT_EQ(tokens[1].type, TokenType::ID);
	EXPECT_EQ(tokens[1].value, "myapp");
	EXPECT_EQ(tokens[2].type, TokenType::DOT);
	EXPECT_EQ(tokens[3].type, TokenType::ID);
	EXPECT_EQ(tokens[3].value, "utils");
	EXPECT_EQ(tokens[4].type, TokenType::SEMICOLON);
}

TEST_F(LexerTest, ActorDeclaration)
{
	std::vector<Token> tokens = GetAllTokens("actor Server { state connections: int; msg connect() {} }");

	EXPECT_EQ(tokens[0].type, TokenType::KW_ACTOR);
	EXPECT_EQ(tokens[1].type, TokenType::ID);
	EXPECT_EQ(tokens[2].type, TokenType::CURLY_OPEN);
	EXPECT_EQ(tokens[3].type, TokenType::KW_STATE);
}

TEST_F(LexerTest, EffectDeclaration)
{
	std::vector<Token> tokens = GetAllTokens("effect IO { fn read() -> string; fn write(s: string); }");

	EXPECT_EQ(tokens[0].type, TokenType::KW_EFFECT);
	EXPECT_EQ(tokens[1].type, TokenType::ID);
}

TEST_F(LexerTest, TypeAlias)
{
	std::vector<Token> tokens = GetAllTokens("type StringList = List<string>;");

	EXPECT_EQ(tokens[0].type, TokenType::KW_TYPE);
	EXPECT_EQ(tokens[1].type, TokenType::ID);
	EXPECT_EQ(tokens[2].type, TokenType::OP_ASSIGNMENT);
	EXPECT_EQ(tokens[3].type, TokenType::ID);
}

TEST_F(LexerTest, ConstDeclaration)
{
	std::vector<Token> tokens = GetAllTokens("const MAX_SIZE: int = 100;");

	EXPECT_EQ(tokens[0].type, TokenType::KW_CONST);
	EXPECT_EQ(tokens[1].type, TokenType::ID);
	EXPECT_EQ(tokens[2].type, TokenType::COLON);
	EXPECT_EQ(tokens[3].type, TokenType::KW_INT);
	EXPECT_EQ(tokens[4].type, TokenType::OP_ASSIGNMENT);
	EXPECT_EQ(tokens[5].type, TokenType::INTEGER_LITERAL);
}

TEST_F(LexerTest, VarDeclaration)
{
	std::vector<Token> tokens = GetAllTokens("var count: int = 0;");

	EXPECT_EQ(tokens[0].type, TokenType::KW_VAR);
	EXPECT_EQ(tokens[1].type, TokenType::ID);
	EXPECT_EQ(tokens[2].type, TokenType::COLON);
	EXPECT_EQ(tokens[3].type, TokenType::KW_INT);
}

TEST_F(LexerTest, SharedModifier)
{
	std::vector<Token> tokens = GetAllTokens("shared var  int;");

	EXPECT_EQ(tokens[0].type, TokenType::KW_SHARED);
	EXPECT_EQ(tokens[1].type, TokenType::KW_VAR);
}

TEST_F(LexerTest, ThreadLocalModifier)
{
	std::vector<Token> tokens = GetAllTokens("thread_local var counter: int;");

	EXPECT_EQ(tokens[0].type, TokenType::KW_THREAD_LOCAL);
	EXPECT_EQ(tokens[1].type, TokenType::KW_VAR);
}

TEST_F(LexerTest, WhileLoop)
{
	std::vector<Token> tokens = GetAllTokens("while (i < 10) { i = i + 1; }");

	EXPECT_EQ(tokens[0].type, TokenType::KW_WHILE);
	EXPECT_EQ(tokens[1].type, TokenType::PARAN_OPEN);
}

TEST_F(LexerTest, IterStatement)
{
	std::vector<Token> tokens = GetAllTokens("iter (x of items) { print(x); }");

	EXPECT_EQ(tokens[0].type, TokenType::KW_ITER);
	EXPECT_EQ(tokens[1].type, TokenType::PARAN_OPEN);
	EXPECT_EQ(tokens[2].type, TokenType::ID);
	EXPECT_EQ(tokens[3].type, TokenType::KW_OF);
}

TEST_F(LexerTest, ReturnStatement)
{
	std::vector<Token> tokens = GetAllTokens("return result;");

	EXPECT_EQ(tokens[0].type, TokenType::KW_RETURN);
	EXPECT_EQ(tokens[1].type, TokenType::ID);
}

TEST_F(LexerTest, UnsafeBlock)
{
	std::vector<Token> tokens = GetAllTokens("unsafe { ptr = 0; }");

	EXPECT_EQ(tokens[0].type, TokenType::KW_UNSAFE);
	EXPECT_EQ(tokens[1].type, TokenType::CURLY_OPEN);
}

TEST_F(LexerTest, ContractRequires)
{
	std::vector<Token> tokens = GetAllTokens("fn div(a: int, b: int) requires (b != 0) -> int {}");

	EXPECT_EQ(tokens[0].type, TokenType::KW_FN);
	bool found = false;
	for (const auto& t : tokens)
	{
		if (t.type == TokenType::KW_REQUIRES)
		{
			found = true;
			break;
		}
	}
	EXPECT_TRUE(found);
}

TEST_F(LexerTest, ContractEnsures)
{
	std::vector<Token> tokens = GetAllTokens("fn abs(x: int) ensures (result >= 0) -> int {}");

	bool found = false;
	for (const auto& t : tokens)
	{
		if (t.type == TokenType::KW_ENSURES)
		{
			found = true;
			break;
		}
	}
	EXPECT_TRUE(found);
}

TEST_F(LexerTest, RaisesEffect)
{
	std::vector<Token> tokens = GetAllTokens("fn read() raises {IO} -> string {}");

	bool found = false;
	for (const auto& t : tokens)
	{
		if (t.type == TokenType::KW_RAISES)
		{
			found = true;
			break;
		}
	}
	EXPECT_TRUE(found);
}

TEST_F(LexerTest, ComptimeBlock)
{
	std::vector<Token> tokens = GetAllTokens("comptime { const X = 1 + 2; }");

	EXPECT_EQ(tokens[0].type, TokenType::KW_COMPTIME);
	EXPECT_EQ(tokens[1].type, TokenType::CURLY_OPEN);
}

TEST_F(LexerTest, HandleStatement)
{
	std::vector<Token> tokens = GetAllTokens("handle op with { effect IO(e) -> {} }");

	EXPECT_EQ(tokens[0].type, TokenType::KW_HANDLE);
	EXPECT_EQ(tokens[2].type, TokenType::KW_WITH);
}

TEST_F(LexerTest, TransactionStatement)
{
	std::vector<Token> tokens = GetAllTokens("transaction (db) { commit(); }");

	EXPECT_EQ(tokens[0].type, TokenType::KW_TRANSACTION);
}

TEST_F(LexerTest, EnumDeclaration)
{
	std::vector<Token> tokens = GetAllTokens("enum Color { Red | Green | Blue }");

	EXPECT_EQ(tokens[0].type, TokenType::KW_ENUM);
	EXPECT_EQ(tokens[1].type, TokenType::ID);
	EXPECT_EQ(tokens[2].type, TokenType::CURLY_OPEN);
	EXPECT_EQ(tokens[3].type, TokenType::ID);
	EXPECT_EQ(tokens[4].type, TokenType::PIPE);
}

TEST_F(LexerTest, InterfaceDeclaration)
{
	std::vector<Token> tokens = GetAllTokens("interface Drawable { fn draw(); }");

	EXPECT_EQ(tokens[0].type, TokenType::KW_INTERFACE);
	EXPECT_EQ(tokens[1].type, TokenType::ID);
}

TEST_F(LexerTest, TypeParameters)
{
	std::vector<Token> tokens = GetAllTokens("fn identity<T>(x: T) -> T { return x; }");

	EXPECT_EQ(tokens[0].type, TokenType::KW_FN);
	EXPECT_EQ(tokens[1].type, TokenType::ID);
	EXPECT_EQ(tokens[2].type, TokenType::OP_LESS);
}

TEST_F(LexerTest, PointerType)
{
	std::vector<Token> tokens = GetAllTokens("var ptr: ptr<int>;");

	EXPECT_EQ(tokens[0].type, TokenType::KW_VAR);
	EXPECT_EQ(tokens[2].type, TokenType::COLON);
	EXPECT_EQ(tokens[3].type, TokenType::KW_PTR);
}

TEST_F(LexerTest, ReferenceType)
{
	std::vector<Token> tokens = GetAllTokens("var ref: ref<int>;");

	EXPECT_EQ(tokens[3].type, TokenType::KW_REF);
}

TEST_F(LexerTest, ArrayType)
{
	std::vector<Token> tokens = GetAllTokens("var arr: [int];");

	EXPECT_EQ(tokens[3].type, TokenType::BRACKET_OPEN);
}

TEST_F(LexerTest, FunctionType)
{
	std::vector<Token> tokens = GetAllTokens("var fn: int -> int;");

	bool found = false;
	for (const auto& t : tokens)
	{
		if (t.type == TokenType::ARROW)
		{
			found = true;
			break;
		}
	}
	EXPECT_TRUE(found);
}

TEST_F(LexerTest, BooleanLiterals)
{
	Token token = GetToken("true");
	EXPECT_EQ(token.type, TokenType::KW_TRUE);

	token = GetToken("false");
	EXPECT_EQ(token.type, TokenType::KW_FALSE);
}

TEST_F(LexerTest, NullLiteral)
{
	Token token = GetToken("null");
	EXPECT_EQ(token.type, TokenType::KW_NULL);
}

TEST_F(LexerTest, VoidType)
{
	Token token = GetToken("void");
	EXPECT_EQ(token.type, TokenType::KW_VOID);
}

TEST_F(LexerTest, NeverType)
{
	Token token = GetToken("never");
	EXPECT_EQ(token.type, TokenType::KW_NEVER);
}

TEST_F(LexerTest, StringType)
{
	Token token = GetToken("string");
	EXPECT_EQ(token.type, TokenType::KW_STRING);
}

TEST_F(LexerTest, AndOperator)
{
	Token token = GetToken("and");
	EXPECT_EQ(token.type, TokenType::KW_AND);
}

TEST_F(LexerTest, OrOperator)
{
	Token token = GetToken("or");
	EXPECT_EQ(token.type, TokenType::KW_OR);
}

TEST_F(LexerTest, NotOperator)
{
	Token token = GetToken("not");
	EXPECT_EQ(token.type, TokenType::KW_NOT);
}

TEST_F(LexerTest, ModOperator)
{
	Token token = GetToken("mod");
	EXPECT_EQ(token.type, TokenType::KW_MOD);
}

TEST_F(LexerTest, DivOperator)
{
	Token token = GetToken("div");
	EXPECT_EQ(token.type, TokenType::KW_DIV);
}

TEST_F(LexerTest, DropAdapter)
{
	Token token = GetToken("drop");
	EXPECT_EQ(token.type, TokenType::KW_DROP);
}

TEST_F(LexerTest, TakeAdapter)
{
	Token token = GetToken("take");
	EXPECT_EQ(token.type, TokenType::KW_TAKE);
}

TEST_F(LexerTest, ReverseAdapter)
{
	Token token = GetToken("reverse");
	EXPECT_EQ(token.type, TokenType::KW_REVERSE);
}

TEST_F(LexerTest, FilterAdapter)
{
	Token token = GetToken("filter");
	EXPECT_EQ(token.type, TokenType::KW_FILTER);
}

TEST_F(LexerTest, TransformAdapter)
{
	Token token = GetToken("transform");
	EXPECT_EQ(token.type, TokenType::KW_TRANSFORM);
}

TEST_F(LexerTest, QueryMethod)
{
	Token token = GetToken("query");
	EXPECT_EQ(token.type, TokenType::KW_QUERY);
}

TEST_F(LexerTest, MsgMethod)
{
	Token token = GetToken("msg");
	EXPECT_EQ(token.type, TokenType::KW_MSG);
}

TEST_F(LexerTest, StateField)
{
	Token token = GetToken("state");
	EXPECT_EQ(token.type, TokenType::KW_STATE);
}

TEST_F(LexerTest, ContextKeyword)
{
	Token token = GetToken("context");
	EXPECT_EQ(token.type, TokenType::KW_CONTEXT);
}

TEST_F(LexerTest, WithKeyword)
{
	Token token = GetToken("with");
	EXPECT_EQ(token.type, TokenType::KW_WITH);
}

TEST_F(LexerTest, InvariantContract)
{
	Token token = GetToken("invariant");
	EXPECT_EQ(token.type, TokenType::KW_INVARIANT);
}

TEST_F(LexerTest, ExportDeclaration)
{
	Token token = GetToken("export");
	EXPECT_EQ(token.type, TokenType::KW_EXPORT);
}

TEST_F(LexerTest, AsKeyword)
{
	Token token = GetToken("as");
	EXPECT_EQ(token.type, TokenType::KW_AS);
}

TEST_F(LexerTest, UnexpectedCharacter)
{
	Token token = GetToken("@");
	EXPECT_EQ(token.type, TokenType::ERROR);
	EXPECT_EQ(token.error, Error::UNEXPECTED_CHARACTER);
}

TEST_F(LexerTest, MultipleTokens)
{
	std::vector<Token> tokens = GetAllTokens("x = 5 + 3;");

	EXPECT_EQ(tokens[0].type, TokenType::ID);
	EXPECT_EQ(tokens[0].value, "x");
	EXPECT_EQ(tokens[1].type, TokenType::OP_ASSIGNMENT);
	EXPECT_EQ(tokens[2].type, TokenType::INTEGER_LITERAL);
	EXPECT_EQ(tokens[2].value, "5");
	EXPECT_EQ(tokens[3].type, TokenType::OP_PLUS);
	EXPECT_EQ(tokens[4].type, TokenType::INTEGER_LITERAL);
	EXPECT_EQ(tokens[4].value, "3");
	EXPECT_EQ(tokens[5].type, TokenType::SEMICOLON);
	EXPECT_EQ(tokens[6].type, TokenType::EOF_TOKEN);
}

TEST_F(LexerTest, ComplexExpression)
{
	std::vector<Token> tokens = GetAllTokens("if (a && b || c) { x = (y + z) * w; }");

	EXPECT_EQ(tokens[0].type, TokenType::KW_IF);
	EXPECT_EQ(tokens[2].type, TokenType::ID);
	EXPECT_EQ(tokens[3].type, TokenType::OP_DOUBLE_AMPERSAND);
	EXPECT_EQ(tokens[4].type, TokenType::ID);
	EXPECT_EQ(tokens[5].type, TokenType::OP_DOUBLE_PIPE);
}

TEST_F(LexerTest, NestedBrackets)
{
	std::vector<Token> tokens = GetAllTokens("[[1, 2], [3, 4]]");

	EXPECT_EQ(tokens[0].type, TokenType::BRACKET_OPEN);
	EXPECT_EQ(tokens[1].type, TokenType::BRACKET_OPEN);
}

TEST_F(LexerTest, ChainedCalls)
{
	std::vector<Token> tokens = GetAllTokens("obj.method().field[0]");

	EXPECT_EQ(tokens[0].type, TokenType::ID);
	EXPECT_EQ(tokens[0].value, "obj");
	EXPECT_EQ(tokens[1].type, TokenType::DOT);
	EXPECT_EQ(tokens[2].type, TokenType::ID);
	EXPECT_EQ(tokens[2].value, "method");
	EXPECT_EQ(tokens[3].type, TokenType::PARAN_OPEN);
}

TEST_F(LexerTest, TypeArguments)
{
	std::vector<Token> tokens = GetAllTokens("Map<string, int>");

	EXPECT_EQ(tokens[0].type, TokenType::ID);
	EXPECT_EQ(tokens[0].value, "Map");
	EXPECT_EQ(tokens[1].type, TokenType::OP_LESS);
	EXPECT_EQ(tokens[2].type, TokenType::KW_STRING);
	EXPECT_EQ(tokens[3].type, TokenType::COMMA);
	EXPECT_EQ(tokens[4].type, TokenType::KW_INT);
	EXPECT_EQ(tokens[5].type, TokenType::OP_GREATER);
}

TEST_F(LexerTest, LambdaFunction)
{
	std::vector<Token> tokens = GetAllTokens("(x, y) -> x + y");

	EXPECT_EQ(tokens[0].type, TokenType::PARAN_OPEN);
	EXPECT_EQ(tokens[1].type, TokenType::ID);
	EXPECT_EQ(tokens[2].type, TokenType::COMMA);
	EXPECT_EQ(tokens[3].type, TokenType::ID);
	EXPECT_EQ(tokens[4].type, TokenType::PARAN_CLOSE);
	EXPECT_EQ(tokens[5].type, TokenType::ARROW);
}

TEST_F(LexerTest, LargeNumber)
{
	Token token = GetToken("1234567890");
	EXPECT_EQ(token.type, TokenType::INTEGER_LITERAL);
	EXPECT_EQ(token.value, "1234567890");
}

TEST_F(LexerTest, FloatWithLeadingZero)
{
	Token token = GetToken("0.123");
	EXPECT_EQ(token.type, TokenType::FLOAT_LITERAL);
	EXPECT_EQ(token.value, "0.123");
}

TEST_F(LexerTest, LongString)
{
	Token token = GetToken("\"This is a very long string with many characters inside it\"");
	EXPECT_EQ(token.type, TokenType::STRING_LITERAL);
	EXPECT_GT(token.value.size(), 50);
}

TEST_F(LexerTest, MixedWhitespace)
{
	Token token = GetToken("  \t\n  \r  variable");
	EXPECT_EQ(token.type, TokenType::ID);
}

TEST_F(LexerTest, CommentAtEnd)
{
	std::vector<Token> tokens = GetAllTokens("x // comment");

	EXPECT_EQ(tokens[0].type, TokenType::ID);
	EXPECT_EQ(tokens[1].type, TokenType::EOF_TOKEN);
}

TEST_F(LexerTest, BlockCommentInCode)
{
	std::vector<Token> tokens = GetAllTokens("x /* comment */ + y");

	EXPECT_EQ(tokens[0].type, TokenType::ID);
	EXPECT_EQ(tokens[1].type, TokenType::OP_PLUS);
	EXPECT_EQ(tokens[2].type, TokenType::ID);
}

TEST_F(LexerTest, MultipleIdentifiers)
{
	std::vector<Token> tokens = GetAllTokens("a b c d e");

	EXPECT_EQ(tokens[0].value, "a");
	EXPECT_EQ(tokens[1].value, "b");
	EXPECT_EQ(tokens[2].value, "c");
	EXPECT_EQ(tokens[3].value, "d");
	EXPECT_EQ(tokens[4].value, "e");
}

TEST_F(LexerTest, AllPrimitiveTypes)
{
	std::vector<Token> tokens = GetAllTokens("int float bool string void never");

	EXPECT_EQ(tokens[0].type, TokenType::KW_INT);
	EXPECT_EQ(tokens[1].type, TokenType::KW_FLOAT);
	EXPECT_EQ(tokens[2].type, TokenType::KW_BOOL);
	EXPECT_EQ(tokens[3].type, TokenType::KW_STRING);
	EXPECT_EQ(tokens[4].type, TokenType::KW_VOID);
	EXPECT_EQ(tokens[5].type, TokenType::KW_NEVER);
}
