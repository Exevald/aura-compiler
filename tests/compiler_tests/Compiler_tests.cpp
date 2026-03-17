#include "../../src/ast/AST.h"
#include "../../src/bytecode/BytecodeGenerator.h"
#include "../../src/lexer/Lexer.h"
#include "../../src/parser/Parser.h"
#include "../../src/vm/core/OpCode.h"

#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

using namespace VM::Core;

class CompilerTest : public ::testing::Test
{
protected:
	static ASTNodePtr ParseCode(const std::string& source)
	{
		std::ifstream file("grammar.txt");
		std::stringstream buffer;
		buffer << file.rdbuf();
		file.close();

		Lexer lexer(source);
		SLRParser parser(lexer, buffer.str());
		if (!parser.Parse())
		{
			return nullptr;
		}
		return parser.GetRoot();
	}
};

TEST_F(CompilerTest, BytecodeSimpleAddition)
{
	auto root = ParseCode("var res = 10 + 20;");
	ASSERT_NE(root, nullptr)
		<< "Parsing failed! Remember: expressions in Aura must be inside declarations or functions.";

	BytecodeGenerator compiler;
	auto chunk = compiler.Compile(root.get());

	ASSERT_GE(chunk.code.size(), 6);

	EXPECT_EQ(static_cast<OpCode>(chunk.code[0]), OpCode::OP_CONSTANT);
	EXPECT_EQ(static_cast<OpCode>(chunk.code[2]), OpCode::OP_CONSTANT);

	EXPECT_EQ(std::get<long long>(chunk.constants[chunk.code[1]]), 10);
	EXPECT_EQ(std::get<long long>(chunk.constants[chunk.code[3]]), 20);

	EXPECT_EQ(static_cast<OpCode>(chunk.code[4]), OpCode::OP_ADD);
	EXPECT_EQ(static_cast<OpCode>(chunk.code[5]), OpCode::OP_SET_LOCAL);
}

TEST_F(CompilerTest, BytecodeVariableDeclaration)
{
	const auto root = ParseCode("var myVar = 100;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	auto chunk = compiler.Compile(root.get());

	bool foundSetLocal = false;
	for (size_t i = 0; i < chunk.code.size(); ++i)
	{
		if (static_cast<OpCode>(chunk.code[i]) == OpCode::OP_SET_LOCAL)
		{
			foundSetLocal = true;
			ASSERT_LT(i + 1, chunk.code.size());
			EXPECT_EQ(chunk.code[i + 1], 0);
		}
	}
	EXPECT_TRUE(foundSetLocal);
}

TEST_F(CompilerTest, BytecodeMathComplexity)
{
	const auto root = ParseCode("var x = 2 + 3 * 4;");
	ASSERT_NE(root, nullptr);

	BytecodeGenerator compiler;
	auto chunk = compiler.Compile(root.get());

	const std::vector expectedSequence = {
		OpCode::OP_CONSTANT,
		OpCode::OP_CONSTANT,
		OpCode::OP_CONSTANT,
		OpCode::OP_MULTIPLY,
		OpCode::OP_ADD,
		OpCode::OP_SET_LOCAL,
		OpCode::OP_RETURN
	};

	size_t seqIdx = 0;
	for (size_t i = 0; i < chunk.code.size(); ++i)
	{
		if (const auto op = static_cast<OpCode>(chunk.code[i]);
			op == expectedSequence[seqIdx])
		{
			seqIdx++;
			if (OpCodeHasOperand(op))
			{
				i++;
			}
		}
		if (seqIdx == expectedSequence.size())
		{
			break;
		}
	}

	EXPECT_EQ(seqIdx, expectedSequence.size());
}

TEST_F(CompilerTest, BytecodeIfElse)
{
	const auto root = ParseCode(
		"fn test() {"
		"  var x = 10;"
		"  if (x > 5) {"
		"    x = 1;"
		"  } else {"
		"    x = 0;"
		"  };"
		"}");

	ASSERT_NE(root, nullptr)
		<< "Parsing failed! 'if' must be inside a function and followed by ';'.";

	BytecodeGenerator compiler;
	const auto chunk = compiler.Compile(root.get());

	bool hasJumpIfFalse = false;
	bool hasJump = false;

	for (const auto byte : chunk.code)
	{
		if (byte == static_cast<uint8_t>(OpCode::OP_JUMP_IF_FALSE))
		{
			hasJumpIfFalse = true;
		}
		if (byte == static_cast<uint8_t>(OpCode::OP_JUMP))
		{
			hasJump = true;
		}
	}

	EXPECT_TRUE(hasJumpIfFalse) << "Missing OP_JUMP_IF_FALSE";
	EXPECT_TRUE(hasJump) << "Missing OP_JUMP (for else branch)";
}